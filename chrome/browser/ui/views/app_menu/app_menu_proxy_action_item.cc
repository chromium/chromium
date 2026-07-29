// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_proxy_action_item.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"

AppMenuProxyActionItem::AppMenuProxyActionItem(actions::ActionItem* delegate)
    : delegate_(delegate) {
  SyncWithDelegate();

  SetInvokeActionCallback(base::BindRepeating(
      [](actions::ActionItem* delegate, actions::ActionItem* proxy,
         actions::ActionInvocationContext context) {
        if (delegate) {
          delegate->InvokeAction(std::move(context));
        }
      },
      base::Unretained(delegate_)));

  if (delegate_) {
    delegate_changed_subscription_ =
        delegate_->AddActionChangedCallback(base::BindRepeating(
            &AppMenuProxyActionItem::SyncWithDelegate, base::Unretained(this)));
  }
}

AppMenuProxyActionItem::~AppMenuProxyActionItem() = default;

void AppMenuProxyActionItem::SyncWithDelegate() {
  if (!delegate_) {
    return;
  }
  auto update = BeginUpdate();

  SetActionId(delegate_->GetActionId());

  SetText(delegate_->GetText());

  SetAccelerator(delegate_->GetAccelerator());
  SetEnabled(delegate_->GetEnabled());
  SetVisible(delegate_->GetVisible());
  SetChecked(delegate_->GetChecked());
  SetTooltipText(delegate_->GetTooltipText());

  ui::ImageModel image = delegate_->GetImage();
  if (image.IsVectorIcon()) {
    image = ui::ImageModel::FromVectorIcon(*image.GetVectorIcon().vector_icon(),
                                           ui::kColorSysPrimary);
  }
  SetImage(image);
}

BEGIN_METADATA(AppMenuProxyActionItem)
END_METADATA
