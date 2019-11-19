// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_domain_list_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/bulleted_label_list_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

ContentSettingDomainListView::ContentSettingDomainListView(
    const base::string16& title,
    const std::set<std::string>& domains) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto title_label = std::make_unique<views::Label>(title);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(title_label.release());

  auto list_view = std::make_unique<BulletedLabelListView>();
  for (const auto& domain : domains)
    list_view->AddLabel(base::UTF8ToUTF16(domain));
  AddChildView(list_view.release());
}
