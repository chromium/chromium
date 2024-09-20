// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_DOWNLOAD_ITEM_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_DOWNLOAD_ITEM_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace download {

class MockDownloadItem : public DownloadItem {
 public:
  MockDownloadItem();
  ~MockDownloadItem() override;

  // Management of observer lists is common in tests. So Add/RemoveObserver
  // methods are not mocks. In addition, any registered observers will receive a
  // OnDownloadDestroyed() notification when the mock is destroyed.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Dispatches an OnDownloadOpened() notification to observers.
  void NotifyObserversDownloadOpened();
  // Dispatches an OnDownloadRemoved() notification to observers.
  void NotifyObserversDownloadRemoved();
  // Dispatches an OnDownloadUpdated() notification to observers.
  void NotifyObserversDownloadUpdated();

  MOCK_METHOD0(UpdateObservers, void());
  MOCK_METHOD0(ValidateDangerousDownload, void());
  MOCK_METHOD0(ValidateInsecureDownload, void());
  MOCK_METHOD1(CopyDownload, void(AcquireFileCallback));
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(Resume, void(bool));
  MOCK_METHOD1(Cancel, void(bool));
  MOCK_METHOD0(Remove, void());
  MOCK_METHOD0(OpenDownload, void());
  MOCK_METHOD0(ShowDownloadInShell, void());
  MOCK_CONST_METHOD0(GetId, uint32_t());
  MOCK_CONST_METHOD0(GetGuid, const std::string&());
  MOCK_CONST_METHOD0(GetState, DownloadState());
  MOCK_CONST_METHOD0(GetLastReason, DownloadInterruptReason());
  MOCK_CONST_METHOD0(IsPaused, bool());
  MOCK_CONST_METHOD0(AllowMetered, bool());
  MOCK_CONST_METHOD0(IsTemporary, bool());
  MOCK_CONST_METHOD0(CanResume, bool());
  MOCK_CONST_METHOD0(IsDone, bool());
  MOCK_CONST_METHOD0(GetBytesWasted, int64_t());
  MOCK_CONST_METHOD0(GetAutoResumeCount, int32_t());
  MOCK_CONST_METHOD0(GetURL, const GURL&());
  MOCK_CONST_METHOD0(GetUrlChain, const std::vector<GURL>&());
  MOCK_CONST_METHOD0(GetOriginalUrl, const GURL&());
  MOCK_CONST_METHOD0(GetReferrerUrl, const GURL&());
  MOCK_CONST_METHOD0(GetSerializedEmbedderDownloadData, const std::string&());
  MOCK_CONST_METHOD0(GetTabUrl, const GURL&());
  MOCK_CONST_METHOD0(GetTabReferrerUrl, const GURL&());
  MOCK_CONST_METHOD0(GetRequestInitiator, const std::optional<url::Origin>&());
  MOCK_CONST_METHOD0(GetSuggestedFilename, std::string());
  MOCK_CONST_METHOD0(GetContentDisposition, std::string());
  MOCK_CONST_METHOD0(GetResponseHeaders,
                     const scoped_refptr<const net::HttpResponseHeaders>&());
  MOCK_CONST_METHOD0(GetMimeType, std::string());
  MOCK_CONST_METHOD0(GetOriginalMimeType, std::string());
  MOCK_CONST_METHOD0(GetReferrerCharset, std::string());
  MOCK_CONST_METHOD0(GetRemoteAddress, std::string());
  MOCK_CONST_METHOD0(HasUserGesture, bool());
  MOCK_CONST_METHOD0(GetTransitionType, ui::PageTransition());
  MOCK_CONST_METHOD0(GetLastModifiedTime, const std::string&());
  MOCK_CONST_METHOD0(GetETag, const std::string&());
  MOCK_CONST_METHOD0(IsSavePackageDownload, bool());
  MOCK_CONST_METHOD0(GetDownloadSource, DownloadSource());
  MOCK_CONST_METHOD0(GetFullPath, const base::FilePath&());
  MOCK_CONST_METHOD0(GetTargetFilePath, const base::FilePath&());
  MOCK_CONST_METHOD0(GetForcedFilePath, const base::FilePath&());
  MOCK_CONST_METHOD0(GetTemporaryFilePath, base::FilePath());
  MOCK_CONST_METHOD0(GetFileNameToReportUser, base::FilePath());
  MOCK_CONST_METHOD0(GetTargetDisposition, TargetDisposition());
  MOCK_CONST_METHOD0(GetHash, const std::string&());
  MOCK_CONST_METHOD0(GetHashState, const std::string&());
  MOCK_CONST_METHOD0(GetFileExternallyRemoved, bool());
  void DeleteFile(base::OnceCallback<void(bool)> cb) override {
    DeleteFile_(cb);
  }
  MOCK_METHOD1(DeleteFile_, void(base::OnceCallback<void(bool)>& cb));
  MOCK_METHOD0(GetDownloadFile, DownloadFile*());
  MOCK_METHOD0(GetRenameHandler, DownloadItemRenameHandler*());
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD0(IsFromExternalApp, bool());
  MOCK_METHOD0(IsMustDownload, bool());
#endif  // BUILDFLAG(IS_ANDROID)
  MOCK_CONST_METHOD0(IsDangerous, bool());
  MOCK_CONST_METHOD0(IsInsecure, bool());
  MOCK_CONST_METHOD0(GetDangerType, DownloadDangerType());
  MOCK_CONST_METHOD0(GetInsecureDownloadStatus, InsecureDownloadStatus());
  MOCK_CONST_METHOD1(TimeRemaining, bool(base::TimeDelta*));
  MOCK_CONST_METHOD0(CurrentSpeed, int64_t());
  MOCK_CONST_METHOD0(PercentComplete, int());
  MOCK_CONST_METHOD0(AllDataSaved, bool());
  MOCK_CONST_METHOD0(GetTotalBytes, int64_t());
  MOCK_CONST_METHOD0(GetReceivedBytes, int64_t());
  MOCK_CONST_METHOD0(GetReceivedSlices,
                     const std::vector<DownloadItem::ReceivedSlice>&());
  MOCK_CONST_METHOD0(GetUploadedBytes, int64_t());
  MOCK_CONST_METHOD0(GetStartTime, base::Time());
  MOCK_CONST_METHOD0(GetEndTime, base::Time());
  MOCK_METHOD0(CanShowInFolder, bool());
  MOCK_METHOD0(CanOpenDownload, bool());
  MOCK_METHOD0(ShouldOpenFileBasedOnExtension, bool());
  MOCK_METHOD0(ShouldOpenFileByPolicyBasedOnExtension, bool());
  MOCK_CONST_METHOD0(GetOpenWhenComplete, bool());
  MOCK_METHOD0(GetAutoOpened, bool());
  MOCK_CONST_METHOD0(GetOpened, bool());
  MOCK_CONST_METHOD0(GetLastAccessTime, base::Time());
  MOCK_CONST_METHOD0(IsTransient, bool());
  MOCK_CONST_METHOD0(RequireSafetyChecks, bool());
  MOCK_CONST_METHOD0(IsParallelDownload, bool());
  MOCK_CONST_METHOD0(GetDownloadCreationType, DownloadCreationType());
  MOCK_CONST_METHOD0(GetCredentialsMode, ::network::mojom::CredentialsMode());
  MOCK_METHOD((const std::optional<net::IsolationInfo>&),
              GetIsolationInfo,
              (),
              (const override));
  MOCK_METHOD2(OnContentCheckCompleted,
               void(DownloadDangerType, DownloadInterruptReason));
  MOCK_METHOD1(SetOpenWhenComplete, void(bool));
  MOCK_METHOD1(SetOpened, void(bool));
  MOCK_METHOD1(SetLastAccessTime, void(base::Time));
  MOCK_METHOD1(SetDisplayName, void(const base::FilePath&));
  MOCK_METHOD1(SetInsecureDownloadStatus, void(InsecureDownloadStatus));
  MOCK_CONST_METHOD1(DebugString, std::string(bool));
  MOCK_METHOD1(SimulateErrorForTesting, void(DownloadInterruptReason));
  MOCK_METHOD2(Rename, void(const base::FilePath&, RenameDownloadCallback));
  MOCK_METHOD1(OnAsyncScanningCompleted, void(DownloadDangerType));

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_DOWNLOAD_ITEM_H_
