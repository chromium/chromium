// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_AVAILABLE_OFFLINE_CONTENT_HELPER_H_
#define CHROME_RENDERER_NET_AVAILABLE_OFFLINE_CONTENT_HELPER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Notice: this file is only included on OS_ANDROID.

// Wraps calls from the renderer thread to the AvailableOfflineContentProvider,
// and records related UMA.
class AvailableOfflineContentHelper {
 public:
  using AvailableContentCallback =
      base::OnceCallback<void(bool list_visible_by_prefs,
                              const std::string& offline_content_json)>;

  AvailableOfflineContentHelper();

  AvailableOfflineContentHelper(const AvailableOfflineContentHelper&) = delete;
  AvailableOfflineContentHelper& operator=(
      const AvailableOfflineContentHelper&) = delete;

  ~AvailableOfflineContentHelper();

  // Fetch available offline content and return a JSON representation.
  // Calls callback once with the return value. An empty string
  // is returned if no offline content is available.
  // Note: A call to Reset, or deletion of this object will prevent the callback
  // from running.
  void FetchAvailableContent(AvailableContentCallback callback);

  // These methods just forward to the AvailableOfflineContentProvider.
  void LaunchItem(const std::string& id, const std::string& name_space);
  void LaunchDownloadsPage();
  void ListVisibilityChanged(bool is_visible);

  // Abort previous requests and free the mojo connection.
  void Reset();

  using Binder = base::RepeatingCallback<void(
      mojo::PendingReceiver<chrome::mojom::AvailableOfflineContentProvider>)>;
  static void OverrideBinderForTesting(Binder binder);

 private:
  void AvailableContentReceived(
      AvailableContentCallback callback,
      bool list_visible_by_prefs,
      std::vector<chrome::mojom::AvailableOfflineContentPtr> content);

  // Binds |provider_| if necessary. Returns true if the provider is bound.
  bool BindProvider();

  mojo::Remote<chrome::mojom::AvailableOfflineContentProvider> provider_;
  // This is the result of the last FetchAvailableContent call. It is retained
  // only so that metrics can be recorded properly on call to LaunchItem().
  std::vector<chrome::mojom::AvailableOfflineContentPtr> fetched_content_;

  // Records if the last received content message indicated that prefetched
  // articles are available or not.
  bool has_prefetched_content_ = false;
};

#endif  // CHROME_RENDERER_NET_AVAILABLE_OFFLINE_CONTENT_HELPER_H_
