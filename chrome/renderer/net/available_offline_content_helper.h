// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_AVAILABLE_OFFLINE_CONTENT_HELPER_H_
#define CHROME_RENDERER_NET_AVAILABLE_OFFLINE_CONTENT_HELPER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/common/available_offline_content.mojom.h"

// Notice: this file is only included on OS_ANDROID.

// Wraps calls from the renderer thread to the AvailableOfflineContentProvider,
// and records related UMA.
class AvailableOfflineContentHelper {
 public:
  AvailableOfflineContentHelper();
  ~AvailableOfflineContentHelper();

  // Fetch available offline content and return a JSON representation.
  // Calls callback once with the return value. An empty string
  // is returned if no offline content is available.
  // Note: A call to Reset, or deletion of this object will prevent the callback
  // from running.
  void FetchAvailableContent(
      base::OnceCallback<void(const std::string& offline_content_json)>
          callback);

  // Fetch summary of available content and return a JSON representation.
  // Calls the callback once with the return value. An empty string
  // is returned if no offline content is available.
  // Note: A call to Reset, or deletion of this object will prevent the callback
  // from running.
  void FetchSummary(
      base::OnceCallback<void(const std::string& content_summary_json)>
          callback);

  // These methods just forward to the AvailableOfflineContentProvider.
  void LaunchItem(const std::string& id, const std::string& name_space);
  void LaunchDownloadsPage();

  // Abort previous requests and free the mojo connection.
  void Reset();

 private:
  void AvailableContentReceived(
      base::OnceCallback<void(const std::string& offline_content_json)>
          callback,
      std::vector<chrome::mojom::AvailableOfflineContentPtr> content);

  void SummaryReceived(
      base::OnceCallback<void(const std::string& content_summary_json)>
          callback,
      chrome::mojom::AvailableOfflineContentSummaryPtr summary);

  // Binds |provider_| if necessary. Returns true if the provider is bound.
  bool BindProvider();

  chrome::mojom::AvailableOfflineContentProviderPtr provider_;
  // This is the result of the last FetchAvailableContent call. It is retained
  // only so that metrics can be recorded properly on call to LaunchItem().
  std::vector<chrome::mojom::AvailableOfflineContentPtr> fetched_content_;

  // Records if the last received content message indicated that prefetched
  // articles are available or not.
  bool has_prefetched_content_ = false;

  DISALLOW_COPY_AND_ASSIGN(AvailableOfflineContentHelper);
};

#endif  // CHROME_RENDERER_NET_AVAILABLE_OFFLINE_CONTENT_HELPER_H_
