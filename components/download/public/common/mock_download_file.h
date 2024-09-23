// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_DOWNLOAD_FILE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_DOWNLOAD_FILE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/download/public/common/download_file.h"
#include "components/download/public/common/input_stream.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace download {

class MockDownloadFile : public DownloadFile {
 public:
  MockDownloadFile();
  ~MockDownloadFile() override;

  // DownloadFile functions.
  // Using the legacy workaround for move-only types in mock methods.
  MOCK_METHOD(void,
              Initialize,
              (InitializeCallback initialize_callback,
               CancelRequestCallback cancel_request_callback,
               const DownloadItem::ReceivedSlices& received_slices),
              ());
  void AddInputStream(std::unique_ptr<InputStream> input_stream,
                      int64_t offset) override;
  MOCK_METHOD2(DoAddInputStream,
               void(InputStream* input_stream, int64_t offset));
  MOCK_METHOD2(OnResponseCompleted,
               void(int64_t offset, DownloadInterruptReason status));
  MOCK_METHOD2(AppendDataToFile,
               DownloadInterruptReason(const char* data, size_t data_len));
  MOCK_METHOD1(Rename,
               DownloadInterruptReason(const base::FilePath& full_path));
  MOCK_METHOD2(RenameAndUniquify,
               void(const base::FilePath& full_path,
                    RenameCompletionCallback callback));
  MOCK_METHOD(
      void,
      RenameAndAnnotate,
      (const base::FilePath& full_path,
       const std::string& client_guid,
       const GURL& source_url,
       const GURL& referrer_url,
       const std::optional<url::Origin>& request_initiator,
       mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
       RenameCompletionCallback callback));
  MOCK_METHOD0(Detach, void());
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD1(SetPotentialFileLength, void(int64_t length));
  MOCK_METHOD0(Finish, void());
  MOCK_CONST_METHOD0(FullPath, const base::FilePath&());
  MOCK_CONST_METHOD0(InProgress, bool());
  MOCK_CONST_METHOD0(BytesSoFar, int64_t());
  MOCK_CONST_METHOD0(CurrentSpeed, int64_t());
  MOCK_METHOD1(GetHash, bool(std::string* hash));
  MOCK_METHOD0(SendUpdate, void());
  MOCK_CONST_METHOD0(Id, int());
  MOCK_CONST_METHOD0(DebugString, std::string());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Resume, void());
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD6(RenameToIntermediateUri,
               void(const GURL& original_url,
                    const GURL& referrer_url,
                    const base::FilePath& file_name,
                    const std::string& mime_type,
                    const base::FilePath& current_path,
                    RenameCompletionCallback callback));
  MOCK_METHOD1(PublishDownload, void(RenameCompletionCallback callback));
  MOCK_METHOD0(GetDisplayName, base::FilePath());
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_DOWNLOAD_FILE_H_
