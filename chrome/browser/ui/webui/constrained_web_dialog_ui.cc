// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/tab_helper.h"
#endif

using content::RenderFrameHost;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

const char kConstrainedWebDialogDelegateUserDataKey[] =
    "ConstrainedWebDialogDelegateUserData";

class ConstrainedWebDialogDelegateUserData
    : public base::SupportsUserData::Data {
 public:
  explicit ConstrainedWebDialogDelegateUserData(
      ConstrainedWebDialogDelegate* delegate) : delegate_(delegate) {}
  ~ConstrainedWebDialogDelegateUserData() override {}

  ConstrainedWebDialogDelegate* delegate() { return delegate_; }

 private:
  ConstrainedWebDialogDelegate* delegate_;  // unowned

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateUserData);
};

}  // namespace

ConstrainedWebDialogUI::ConstrainedWebDialogUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::TabHelper::CreateForWebContents(web_ui->GetWebContents());
#endif
}

ConstrainedWebDialogUI::~ConstrainedWebDialogUI() {
}

void ConstrainedWebDialogUI::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Add a "dialogClose" callback which matches WebDialogUI behavior.
  web_ui()->RegisterMessageCallback(
      "dialogClose",
      base::BindRepeating(&ConstrainedWebDialogUI::OnDialogCloseMessage,
                          base::Unretained(this)));

  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;

  ui::WebDialogDelegate* dialog_delegate = delegate->GetWebDialogDelegate();
  std::vector<WebUIMessageHandler*> handlers;
  dialog_delegate->GetWebUIMessageHandlers(&handlers);
  render_frame_host->SetWebUIProperty("dialogArguments",
                                      dialog_delegate->GetDialogArgs());
  for (WebUIMessageHandler* handler : handlers) {
    web_ui()->AddMessageHandler(base::WrapUnique(handler));
  }

  dialog_delegate->OnDialogShown(web_ui());
}

void ConstrainedWebDialogUI::OnDialogCloseMessage(const base::ListValue* args) {
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;

  std::string json_retval;
  if (!args->empty() && !args->GetString(0, &json_retval))
    NOTREACHED() << "Could not read JSON argument";
  delegate->GetWebDialogDelegate()->OnDialogClosed(json_retval);
  delegate->OnDialogCloseFromWebUI();
}

// static
void ConstrainedWebDialogUI::SetConstrainedDelegate(
    content::WebContents* web_contents,
    ConstrainedWebDialogDelegate* delegate) {
  web_contents->SetUserData(
      &kConstrainedWebDialogDelegateUserDataKey,
      std::make_unique<ConstrainedWebDialogDelegateUserData>(delegate));
}

// static
void ConstrainedWebDialogUI::ClearConstrainedDelegate(
    content::WebContents* web_contents) {
  web_contents->RemoveUserData(&kConstrainedWebDialogDelegateUserDataKey);
}

ConstrainedWebDialogDelegate* ConstrainedWebDialogUI::GetConstrainedDelegate() {
  ConstrainedWebDialogDelegateUserData* user_data =
      static_cast<ConstrainedWebDialogDelegateUserData*>(
          web_ui()->GetWebContents()->
              GetUserData(&kConstrainedWebDialogDelegateUserDataKey));

  return user_data ? user_data->delegate() : NULL;
}
