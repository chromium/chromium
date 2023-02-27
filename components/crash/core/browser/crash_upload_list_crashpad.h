// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_BROWSER_CRASH_UPLOAD_LIST_CRASHPAD_H_
#define COMPONENTS_CRASH_CORE_BROWSER_CRASH_UPLOAD_LIST_CRASHPAD_H_

#include "components/upload_list/upload_list.h"

namespace base {
class Time;
}

// An UploadList that retrieves the list of crash reports from the
// Crashpad database.
class CrashUploadListCrashpad : public UploadList {
 public:
  CrashUploadListCrashpad();

 protected:
  ~CrashUploadListCrashpad() override;

  std::vector<std::unique_ptr<UploadList::UploadInfo>> LoadUploadList()
      override;
  void ClearUploadList(const base::Time& begin, const base::Time& end) override;
  void RequestSingleUpload(const std::string& local_id) override;

  CrashUploadListCrashpad(const CrashUploadListCrashpad&) = delete;
  CrashUploadListCrashpad& operator=(const CrashUploadListCrashpad&) = delete;
};

#endif  // COMPONENTS_CRASH_CORE_BROWSER_CRASH_UPLOAD_LIST_CRASHPAD_H_
