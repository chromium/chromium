// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_TEST_TEST_UTIL_H_
#define COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_TEST_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace on_device_translation {

// Returns the path to a mock library that can be used for testing.
// The library is a valid library, but it will not do anything useful.
// Its translate method returns the concatenation of the content of "dict.dat"
// in the passed language pack and the input text. See comments in
// mock_translate_kit_lib.cc for more details.
base::FilePath GetMockLibraryPath();

// Returns the path to a mock library that can be used for testing.
// This library does not contain any of the methods in the TranslateKit API.
// So loading it will cause the library load failure with InvalidFunctionPointer
// error.
base::FilePath GetMockInvalidFunctionPointerLibraryPath();

// Returns the path to a mock library that can be used for testing.
// The library does contain all of the methods in the TranslateKit API, but
// its CreateTranslateKit() method always fails.
base::FilePath GetMockFailingLibraryPath();

// A file to be used in the fake file system.
struct TestFile {
  // The relative path of the file.
  std::string relative_path;
  // The content of the file.
  std::string content;
};

// Sets up the data directory with the given `files`.
base::ScopedTempDir SetupDataDir(const std::vector<TestFile>& files);

// A fake implementation of FileOperationProxy for testing.
class FakeFileOperationProxy : public mojom::FileOperationProxy {
 public:
  // Creates a FakeFileOperationProxy with the given `files`.
  static std::unique_ptr<FakeFileOperationProxy, base::OnTaskRunnerDeleter>
  Create(mojo::PendingReceiver<mojom::FileOperationProxy> proxy_receiver,
         const std::vector<TestFile>& files);

  FakeFileOperationProxy(
      mojo::PendingReceiver<mojom::FileOperationProxy> proxy_receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const std::vector<TestFile>& files,
      base::PassKey<FakeFileOperationProxy>);
  ~FakeFileOperationProxy() override;

  // mojom::FileOperationProxy overrides:
  void FileExists(uint32_t package_index,
                  const base::FilePath& relative_path,
                  FileExistsCallback callback) override;
  void Open(uint32_t package_index,
            const base::FilePath& relative_path,
            OpenCallback callback) override;

 private:
  // Returns the file path for the given `package_index` and `relative_path`.
  base::FilePath GetFilePath(uint32_t package_index,
                             const base::FilePath& relative_path);

  mojo::Receiver<mojom::FileOperationProxy> receiver_{this};
  base::ScopedTempDir package_dir_;
};

}  // namespace on_device_translation

#endif  // COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_TEST_TEST_UTIL_H_
