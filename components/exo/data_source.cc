// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_source.h"

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/i18n/character_encoding.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/data_source_observer.h"
#include "components/exo/mime_utils.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/icu/source/common/unicode/ucnv.h"

namespace exo {

namespace {

constexpr char kTextPlain[] = "text/plain";
constexpr char kTextRTF[] = "text/rtf";
constexpr char kTextHTML[] = "text/html";

constexpr char kUtfPrefix[] = "UTF";
constexpr char kEncoding16[] = "16";
constexpr char kEncodingASCII[] = "ASCII";

constexpr char kUTF16Unspecified[] = "UTF-16";
constexpr char kUTF16LittleEndian[] = "UTF-16LE";
constexpr char kUTF16BigEndian[] = "UTF-16BE";
constexpr uint8_t kByteOrderMark[] = {0xFE, 0xFF};
constexpr int kByteOrderMarkSize = sizeof(kByteOrderMark);

constexpr char kImageBitmap[] = "image/bmp";
constexpr char kImagePNG[] = "image/png";
constexpr char kImageAPNG[] = "image/apng";

std::vector<uint8_t> ReadDataOnWorkerThread(base::ScopedFD fd) {
  constexpr size_t kChunkSize = 1024;
  std::vector<uint8_t> bytes;
  while (true) {
    uint8_t chunk[kChunkSize];
    ssize_t bytes_read = HANDLE_EINTR(read(fd.get(), chunk, kChunkSize));
    if (bytes_read > 0) {
      bytes.insert(bytes.end(), chunk, chunk + bytes_read);
      continue;
    }
    if (!bytes_read)
      return bytes;
    if (bytes_read < 0) {
      PLOG(ERROR) << "Failed to read selection data from clipboard";
      return std::vector<uint8_t>();
    }
  }
}

// Map a named character set to an integer ranking, lower is better. This is an
// implementation detail of DataSource::GetPreferredMimeTypes and should not be
// considered to have any greater meaning. In particular, these are not expected
// to remain stable over time.
int GetCharsetRank(const std::string& charset_input) {
  std::string charset = base::ToUpperASCII(charset_input);

  // Prefer UTF-16 over all other encodings, because that's what the clipboard
  // interface takes as input; then other unicode encodings; then any non-ASCII
  // encoding, because most or all such encodings are super-sets of ASCII;
  // finally, only use ASCII if nothing else is available.
  if (base::StartsWith(charset, kUtfPrefix, base::CompareCase::SENSITIVE)) {
    if (charset.find(kEncoding16) != std::string::npos)
      return 0;
    return 1;
  } else if (charset.find(kEncodingASCII) == std::string::npos) {
    return 2;
  }
  return 3;
}

// Map an image MIME type to an integer ranking, lower is better. This is an
// implementation detail of DataSource::GetPreferredMimeTypes and should not be
// considered to have any greater meaning. In particular, these are not expected
// to remain stable over time.
int GetImageTypeRank(const std::string& mime_type) {
  // Prefer bitmaps most of all to avoid needing to decode the image, followed
  // by other lossless formats, followed by any other format we support.
  if (net::MatchesMimeType(std::string(kImageBitmap), mime_type))
    return 0;
  if (net::MatchesMimeType(std::string(kImagePNG), mime_type) ||
      net::MatchesMimeType(std::string(kImageAPNG), mime_type))
    return 1;
  return 2;
}

base::string16 CodepageToUTF16(const std::vector<uint8_t>& data,
                               const std::string& charset_input) {
  base::string16 output;
  base::StringPiece piece(reinterpret_cast<const char*>(data.data()),
                          data.size());
  const char* charset = charset_input.c_str();

  // Despite claims in the documentation to the contrary, the ICU UTF-16
  // converter does not automatically detect and interpret the byte order
  // mark. Therefore, we must do this ourselves.
  if (!ucnv_compareNames(charset, kUTF16Unspecified) &&
      data.size() >= kByteOrderMarkSize) {
    if (static_cast<uint8_t>(piece.data()[0]) == kByteOrderMark[0] &&
        static_cast<uint8_t>(piece.data()[1]) == kByteOrderMark[1]) {
      // BOM is in big endian format. Consume the BOM so it doesn't get
      // interpreted as a character.
      piece.remove_prefix(2);
      charset = kUTF16BigEndian;
    } else if (static_cast<uint8_t>(piece.data()[0]) == kByteOrderMark[1] &&
               static_cast<uint8_t>(piece.data()[1]) == kByteOrderMark[0]) {
      // BOM is in little endian format. Consume the BOM so it doesn't get
      // interpreted as a character.
      piece.remove_prefix(2);
      charset = kUTF16LittleEndian;
    }
  }

  base::CodepageToUTF16(
      piece, charset, base::OnStringConversionError::Type::SUBSTITUTE, &output);
  return output;
}

}  // namespace

ScopedDataSource::ScopedDataSource(DataSource* data_source,
                                   DataSourceObserver* observer)
    : data_source_(data_source), observer_(observer) {
  data_source_->AddObserver(observer_);
}

ScopedDataSource::~ScopedDataSource() {
  data_source_->RemoveObserver(observer_);
}

DataSource::DataSource(DataSourceDelegate* delegate)
    : delegate_(delegate), finished_(false) {}

DataSource::~DataSource() {
  delegate_->OnDataSourceDestroying(this);
  for (DataSourceObserver& observer : observers_) {
    observer.OnDataSourceDestroying(this);
  }
}

void DataSource::AddObserver(DataSourceObserver* observer) {
  observers_.AddObserver(observer);
}

void DataSource::RemoveObserver(DataSourceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DataSource::Offer(const std::string& mime_type) {
  mime_types_.insert(mime_type);
}

void DataSource::SetActions(const base::flat_set<DndAction>& dnd_actions) {
  dnd_actions_ = dnd_actions;
}

void DataSource::Target(const base::Optional<std::string>& mime_type) {
  delegate_->OnTarget(mime_type);
}

void DataSource::Action(DndAction action) {
  delegate_->OnAction(action);
}

void DataSource::DndDropPerformed() {
  delegate_->OnDndDropPerformed();
}

void DataSource::Cancelled() {
  finished_ = true;
  read_data_weak_ptr_factory_.InvalidateWeakPtrs();
  delegate_->OnCancelled();
}

void DataSource::DndFinished() {
  finished_ = true;
  read_data_weak_ptr_factory_.InvalidateWeakPtrs();
  delegate_->OnDndFinished();
}

void DataSource::ReadDataForTesting(const std::string& mime_type,
                                    ReadDataCallback callback) {
  ReadData(mime_type, std::move(callback), base::DoNothing());
}

void DataSource::ReadData(const std::string& mime_type,
                          ReadDataCallback callback,
                          base::OnceClosure failure_callback) {
  // This DataSource does not contain the requested MIME type.
  if (mime_type.empty() || !mime_types_.count(mime_type) || finished_) {
    std::move(failure_callback).Run();
    return;
  }

  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  PCHECK(base::CreatePipe(&read_fd, &write_fd));
  delegate_->OnSend(mime_type, std::move(write_fd));

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadDataOnWorkerThread, std::move(read_fd)),
      base::BindOnce(&DataSource::OnDataRead,
                     read_data_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), mime_type));
}

void DataSource::OnDataRead(ReadDataCallback callback,
                            const std::string& mime_type,
                            const std::vector<uint8_t>& data) {
  std::move(callback).Run(mime_type, data);
}

void DataSource::GetDataForPreferredMimeTypes(
    ReadTextDataCallback text_reader,
    ReadDataCallback rtf_reader,
    ReadTextDataCallback html_reader,
    ReadDataCallback image_reader,
    base::RepeatingClosure failure_callback) {
  std::string text_mime, rtf_mime, html_mime, image_mime;

  int text_rank = std::numeric_limits<int>::max();
  int html_rank = std::numeric_limits<int>::max();
  int image_rank = std::numeric_limits<int>::max();

  for (auto mime_type : mime_types_) {
    if (net::MatchesMimeType(std::string(kTextPlain), mime_type) ||
        mime_type == kEncodingUTF8Legacy) {
      if (text_reader.is_null())
        continue;

      std::string charset;
      charset = GetCharset(mime_type);
      int new_rank = GetCharsetRank(charset);
      if (new_rank < text_rank) {
        text_mime = mime_type;
        text_rank = new_rank;
      }
    } else if (net::MatchesMimeType(std::string(kTextRTF), mime_type)) {
      if (rtf_reader.is_null())
        continue;

      // The RTF MIME type will never have a character set because it only uses
      // 7-bit bytes and stores character set information internally.
      rtf_mime = mime_type;
    } else if (net::MatchesMimeType(std::string(kTextHTML), mime_type)) {
      if (html_reader.is_null())
        continue;

      auto charset = GetCharset(mime_type);
      int new_rank = GetCharsetRank(charset);
      if (new_rank < html_rank) {
        html_mime = mime_type;
        html_rank = new_rank;
      }
    } else if (blink::IsSupportedImageMimeType(mime_type)) {
      if (image_reader.is_null())
        continue;

      int new_rank = GetImageTypeRank(mime_type);
      if (new_rank < image_rank) {
        image_mime = mime_type;
        image_rank = new_rank;
      }
    }
  }

  ReadData(text_mime,
           base::BindOnce(&DataSource::OnTextRead,
                          read_data_weak_ptr_factory_.GetWeakPtr(),
                          std::move(text_reader)),
           failure_callback);
  ReadData(rtf_mime, std::move(rtf_reader), failure_callback);
  ReadData(html_mime,
           base::BindOnce(&DataSource::OnTextRead,
                          read_data_weak_ptr_factory_.GetWeakPtr(),
                          std::move(html_reader)),
           failure_callback);
  ReadData(image_mime, std::move(image_reader), failure_callback);
}

void DataSource::OnTextRead(ReadTextDataCallback callback,
                            const std::string& mime_type,
                            const std::vector<uint8_t>& data) {
  base::string16 output = CodepageToUTF16(data, GetCharset(mime_type));
  std::move(callback).Run(mime_type, std::move(output));
}

}  // namespace exo
