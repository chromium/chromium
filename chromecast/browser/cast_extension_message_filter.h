// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_EXTENSION_MESSAGE_FILTER_H_
#define CHROMECAST_BROWSER_CAST_EXTENSION_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/task/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/extension_l10n_util.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class InfoMap;
}

// This class filters out incoming Cast-specific IPC messages from the
// extension process on the IPC thread.
class CastExtensionMessageFilter : public content::BrowserMessageFilter {
 public:
  CastExtensionMessageFilter(int render_process_id,
                             content::BrowserContext* context);

  CastExtensionMessageFilter(const CastExtensionMessageFilter&) = delete;
  CastExtensionMessageFilter& operator=(const CastExtensionMessageFilter&) =
      delete;

  // content::BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<CastExtensionMessageFilter>;

  ~CastExtensionMessageFilter() override;

  void OnGetExtMessageBundle(const std::string& extension_id,
                             IPC::Message* reply_msg);
  void OnGetExtMessageBundleAsync(
      const std::vector<base::FilePath>& extension_paths,
      const std::string& main_extension_id,
      const std::string& default_locale,
      extension_l10n_util::GzippedMessagesPermission gzip_permission,
      IPC::Message* reply_msg);

  const int render_process_id_;
  content::BrowserContext* context_;

  scoped_refptr<extensions::InfoMap> extension_info_map_;
};

#endif  // CHROMECAST_BROWSER_CAST_EXTENSION_MESSAGE_FILTER_H_
