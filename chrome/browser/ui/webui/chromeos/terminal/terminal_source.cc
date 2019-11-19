// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/terminal/terminal_source.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/mime_util.h"
#include "third_party/zlib/google/compression_utils.h"

namespace {
// TODO(crbug.com/846546): Initially set to load crosh, but change to
// terminal when it is available.
constexpr base::FilePath::CharType kTerminalRoot[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/crosh_builtin");
constexpr base::FilePath::CharType kDefaultFile[] =
    FILE_PATH_LITERAL("html/crosh.html");
constexpr char kDefaultMime[] = "text/html";

void ReadFile(const base::FilePath& path,
              const content::URLDataSource::GotDataCallback& callback) {
  std::string content;
  // First look for uncompressed resource, then try for gzipped file.
  bool result = base::ReadFileToString(path, &content);
  if (!result) {
    result =
        base::ReadFileToString(base::FilePath(path.value() + ".gz"), &content);
    std::string uncompressed;
    result = compression::GzipUncompress(content, &uncompressed);
    content = std::move(uncompressed);
  }
  // Allow missing files in <root>/_locales only.
  DCHECK(result || base::FilePath(kTerminalRoot)
                       .Append("_locales")
                       .AppendRelativePath(path, nullptr))
      << path;
  scoped_refptr<base::RefCountedString> response =
      base::RefCountedString::TakeString(&content);
  callback.Run(response.get());
}
}  // namespace

std::string TerminalSource::GetSource() {
  return chrome::kChromeUITerminalHost;
}

#if !BUILDFLAG(OPTIMIZE_WEBUI)
bool TerminalSource::AllowCaching() {
  return false;
}
#endif

void TerminalSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  // TODO(crbug/1009127): Simplify usages of |path| since |url| is available.
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  // Reparse path to strip any query or fragment, skip first '/' in path.
  std::string reparsed =
      GURL(chrome::kChromeUITerminalURL + path).path().substr(1);
  if (reparsed.empty())
    reparsed = kDefaultFile;
  base::FilePath file_path = base::FilePath(kTerminalRoot).Append(reparsed);
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFile, file_path, callback));
}

std::string TerminalSource::GetMimeType(const std::string& path) {
  std::string mime_type(kDefaultMime);
  std::string ext = base::FilePath(path).Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  return mime_type;
}

bool TerminalSource::ShouldServeMimeTypeAsContentTypeHeader() {
  // TerminalSource pages include js modules which require an explicit MimeType.
  return true;
}
