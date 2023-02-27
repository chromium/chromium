// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPLOAD_LIST_COMBINING_UPLOAD_LIST_H_
#define COMPONENTS_UPLOAD_LIST_COMBINING_UPLOAD_LIST_H_

#include <vector>

#include "components/upload_list/upload_list.h"

// Presents a combined view of multiple UploadLists as if they were a single
// UploadList
class CombiningUploadList : public UploadList {
 public:
  // Initialize the CombiningUploadList with all the UploadLists it should
  // combine. Note: If one UploadList produces more UploadInfos than the others,
  // it is very slightly more efficient to put that one first.
  explicit CombiningUploadList(std::vector<scoped_refptr<UploadList>> sublists);

 protected:
  ~CombiningUploadList() override;
  std::vector<std::unique_ptr<UploadInfo>> LoadUploadList() override;
  void ClearUploadList(const base::Time& begin, const base::Time& end) override;
  void RequestSingleUpload(const std::string& local_id) override;

 private:
  const std::vector<scoped_refptr<UploadList>> sublists_;
};

#endif  // COMPONENTS_UPLOAD_LIST_COMBINING_UPLOAD_LIST_H_
