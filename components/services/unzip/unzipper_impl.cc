// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/unzipper_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/zlib/google/zip.h"
#include "third_party/zlib/google/zip_reader.h"

namespace unzip {

namespace {

// A writer delegate that reports errors instead of writing.
class DudWriterDelegate : public zip::WriterDelegate {
 public:
  DudWriterDelegate() {}
  ~DudWriterDelegate() override {}

  // WriterDelegate methods:
  bool PrepareOutput() override { return false; }
  bool WriteBytes(const char* data, int num_bytes) override { return false; }
  void SetTimeModified(const base::Time& time) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DudWriterDelegate);
};

std::string PathToMojoString(const base::FilePath& path) {
#if defined(OS_WIN)
  return base::WideToUTF8(path.value());
#else
  return path.value();
#endif
}

// Modifies output_dir to point to the final directory.
bool CreateDirectory(filesystem::mojom::Directory* output_dir,
                     const base::FilePath& path) {
  base::File::Error err = base::File::Error::FILE_OK;
  return output_dir->OpenDirectory(PathToMojoString(path), mojo::NullReceiver(),
                                   filesystem::mojom::kFlagOpenAlways, &err) &&
         err == base::File::Error::FILE_OK;
}

std::unique_ptr<zip::WriterDelegate> MakeFileWriterDelegateNoParent(
    filesystem::mojom::Directory* output_dir,
    const base::FilePath& path) {
  auto file = std::make_unique<base::File>();
  base::File::Error err;
  if (!output_dir->OpenFileHandle(PathToMojoString(path),
                                  filesystem::mojom::kFlagCreate |
                                      filesystem::mojom::kFlagWrite |
                                      filesystem::mojom::kFlagWriteAttributes,
                                  &err, file.get()) ||
      err != base::File::Error::FILE_OK) {
    return std::make_unique<DudWriterDelegate>();
  }
  return std::make_unique<zip::FileWriterDelegate>(std::move(file));
}

std::unique_ptr<zip::WriterDelegate> MakeFileWriterDelegate(
    filesystem::mojom::Directory* output_dir,
    const base::FilePath& path) {
  if (path == path.BaseName())
    return MakeFileWriterDelegateNoParent(output_dir, path);
  mojo::Remote<filesystem::mojom::Directory> parent;
  base::File::Error err;
  if (!output_dir->OpenDirectory(PathToMojoString(path.DirName()),
                                 parent.BindNewPipeAndPassReceiver(),
                                 filesystem::mojom::kFlagOpenAlways, &err) ||
      err != base::File::Error::FILE_OK) {
    return std::make_unique<DudWriterDelegate>();
  }
  return MakeFileWriterDelegateNoParent(parent.get(), path.BaseName());
}

bool FilterNoFiles(const base::FilePath& unused) {
  return true;
}

bool FilterWithFilterRemote(mojom::UnzipFilter* filter,
                            const base::FilePath& path) {
  bool result = false;
  filter->ShouldUnzipFile(path, &result);
  return result;
}

}  // namespace

UnzipperImpl::UnzipperImpl() = default;

UnzipperImpl::UnzipperImpl(mojo::PendingReceiver<mojom::Unzipper> receiver)
    : receiver_(this, std::move(receiver)) {}

UnzipperImpl::~UnzipperImpl() = default;

void UnzipperImpl::Unzip(
    base::File zip_file,
    mojo::PendingRemote<filesystem::mojom::Directory> output_dir_remote,
    UnzipCallback callback) {
  DCHECK(zip_file.IsValid());
  mojo::Remote<filesystem::mojom::Directory> output_dir(
      std::move(output_dir_remote));
  std::move(callback).Run(zip::UnzipWithFilterAndWriters(
      zip_file.GetPlatformFile(),
      base::BindRepeating(&MakeFileWriterDelegate, output_dir.get()),
      base::BindRepeating(&CreateDirectory, output_dir.get()),
      base::BindRepeating(&FilterNoFiles), /*log_skipped_files=*/false));
}

void UnzipperImpl::UnzipWithFilter(
    base::File zip_file,
    mojo::PendingRemote<filesystem::mojom::Directory> output_dir_remote,
    mojo::PendingRemote<mojom::UnzipFilter> filter_remote,
    UnzipCallback callback) {
  DCHECK(zip_file.IsValid());
  mojo::Remote<filesystem::mojom::Directory> output_dir(
      std::move(output_dir_remote));
  mojo::Remote<mojom::UnzipFilter> filter(std::move(filter_remote));

  // Note that we pass a pointer to |filter| below, as it is a repeating
  // callback and transferring its value would cause the callback to fail when
  // called more than once. It is safe to pass a pointer as
  // UnzipWithFilterAndWriters is synchronous, so |filter| won't be used when
  // the method returns.
  std::move(callback).Run(zip::UnzipWithFilterAndWriters(
      zip_file.GetPlatformFile(),
      base::BindRepeating(&MakeFileWriterDelegate, output_dir.get()),
      base::BindRepeating(&CreateDirectory, output_dir.get()),
      base::BindRepeating(&FilterWithFilterRemote, filter.get()),
      /*log_skipped_files=*/false));
}

}  // namespace unzip
