// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_extension_message_filter.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/message_bundle.h"

using content::BrowserThread;

namespace {

const uint32_t kExtensionFilteredMessageClasses[] = {
    ExtensionMsgStart,
};

}  // namespace

CastExtensionMessageFilter::CastExtensionMessageFilter(
    int render_process_id,
    content::BrowserContext* context)
    : BrowserMessageFilter(kExtensionFilteredMessageClasses,
                           base::size(kExtensionFilteredMessageClasses)),
      render_process_id_(render_process_id),
      context_(context),
      extension_info_map_(
          extensions::ExtensionSystem::Get(context)->info_map()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CastExtensionMessageFilter::~CastExtensionMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool CastExtensionMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CastExtensionMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ExtensionHostMsg_GetMessageBundle,
                                    OnGetExtMessageBundle)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CastExtensionMessageFilter::OnDestruct() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
  } else {
    base::DeleteSoon(FROM_HERE, {BrowserThread::UI}, this);
  }
}

void CastExtensionMessageFilter::OnGetExtMessageBundle(
    const std::string& extension_id,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const extensions::ExtensionSet& extension_set =
      extension_info_map_->extensions();
  const extensions::Extension* extension = extension_set.GetByID(extension_id);

  if (!extension) {  // The extension has gone.
    ExtensionHostMsg_GetMessageBundle::WriteReplyParams(
        reply_msg, extensions::MessageBundle::SubstitutionMap());
    Send(reply_msg);
    return;
  }

  const std::string& default_locale =
      extensions::LocaleInfo::GetDefaultLocale(extension);
  if (default_locale.empty()) {
    // A little optimization: send the answer here to avoid an extra thread hop.
    std::unique_ptr<extensions::MessageBundle::SubstitutionMap> dictionary_map(
        extensions::file_util::LoadNonLocalizedMessageBundleSubstitutionMap(
            extension_id));
    ExtensionHostMsg_GetMessageBundle::WriteReplyParams(reply_msg,
                                                        *dictionary_map);
    Send(reply_msg);
    return;
  }

  std::vector<base::FilePath> paths_to_load;
  paths_to_load.push_back(extension->path());

  auto imports = extensions::SharedModuleInfo::GetImports(extension);
  // Iterate through the imports in reverse.  This will allow later imported
  // modules to override earlier imported modules, as the list order is
  // maintained from the definition in manifest.json of the imports.
  for (auto it = imports.rbegin(); it != imports.rend(); ++it) {
    const extensions::Extension* imported_extension =
        extension_set.GetByID(it->extension_id);
    if (!imported_extension) {
      NOTREACHED() << "Missing shared module " << it->extension_id;
      continue;
    }
    paths_to_load.push_back(imported_extension->path());
  }

  // This blocks tab loading. Priority is inherited from the calling context.
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&CastExtensionMessageFilter::OnGetExtMessageBundleAsync,
                     this, paths_to_load, extension_id, default_locale,
                     reply_msg));
}

void CastExtensionMessageFilter::OnGetExtMessageBundleAsync(
    const std::vector<base::FilePath>& extension_paths,
    const std::string& main_extension_id,
    const std::string& default_locale,
    IPC::Message* reply_msg) {
  std::unique_ptr<extensions::MessageBundle::SubstitutionMap> dictionary_map(
      extensions::file_util::LoadMessageBundleSubstitutionMapFromPaths(
          extension_paths, main_extension_id, default_locale));

  ExtensionHostMsg_GetMessageBundle::WriteReplyParams(reply_msg,
                                                      *dictionary_map);
  Send(reply_msg);
}
