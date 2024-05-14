// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_UI_DELEGATE_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_UI_DELEGATE_H_

namespace data_sharing {

// An interface for the data sharing service to communicate with UI elements.
class DataSharingUIDelegate {
 public:
  DataSharingUIDelegate() = default;
  virtual ~DataSharingUIDelegate() = default;

  // Handle the intercepted URL to show relevant data sharing group information.
  virtual void HandleShareURLIntercepted(const GURL& url) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_UI_DELEGATE_H_
