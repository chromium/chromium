// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/alternate_nav_infobar_view.h"

#include <stddef.h>

#include <utility>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"

// AlternateNavInfoBarDelegate -------------------------------------------------

// static
std::unique_ptr<infobars::InfoBar> AlternateNavInfoBarDelegate::CreateInfoBar(
    std::unique_ptr<AlternateNavInfoBarDelegate> delegate) {
  return std::make_unique<AlternateNavInfoBarView>(std::move(delegate));
}

// AlternateNavInfoBarView -----------------------------------------------------

AlternateNavInfoBarView::AlternateNavInfoBarView(
    std::unique_ptr<AlternateNavInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {
  auto* delegate_ptr = GetDelegate();
  size_t offset;
  std::u16string message_text = delegate_ptr->GetMessageTextWithOffset(&offset);
  DCHECK_NE(std::u16string::npos, offset);
  label_1_text_ = message_text.substr(0, offset);

  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    label_1_ = content_container()->AddChildView(CreateLabel(label_1_text_));
  } else {
    label_1_ = AddContentChildView(CreateLabel(label_1_text_));
  }

  link_text_ = delegate_ptr->GetLinkText();
  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    link_ = content_container()->AddChildView(CreateLink(link_text_));
  } else {
    link_ = AddContentChildView(CreateLink(link_text_));
  }

  label_2_text_ = message_text.substr(offset);
  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    label_2_ = content_container()->AddChildView(CreateLabel(label_2_text_));
  } else {
    label_2_ = AddContentChildView(CreateLabel(label_2_text_));
  }
}

AlternateNavInfoBarView::~AlternateNavInfoBarView() = default;

// static
void AlternateNavInfoBarView::ElideLabels(Labels* labels, int available_width) {
  views::Label* last_label = labels->back();
  labels->pop_back();
  int used_width = 0;
  for (auto& label : *labels) {
    used_width +=
        label->GetPreferredSize(views::SizeBounds(label->width(), {})).width();
  }
  int last_label_width = std::min(
      last_label->GetPreferredSize(views::SizeBounds(last_label->width(), {}))
          .width(),
      available_width - used_width);
  if (last_label_width < last_label->GetMinimumSize().width()) {
    last_label_width = 0;
    if (!labels->empty()) {
      labels->back()->SetText(
          base::StrCat({labels->back()->GetText(), gfx::kEllipsisUTF16}));
    }
  }
  last_label->SetSize(gfx::Size(last_label_width, last_label->height()));
  if (!labels->empty()) {
    ElideLabels(labels, available_width - last_label_width);
  }
}

void AlternateNavInfoBarView::Layout(PassKey) {
  LayoutSuperclass<InfoBarView>(this);

  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    return;
  }
  label_1_->SetText(label_1_text_);
  link_->SetText(link_text_);
  label_2_->SetText(label_2_text_);
  Labels labels;
  labels.push_back(label_1_);
  labels.push_back(link_);
  labels.push_back(label_2_);
  ElideLabels(&labels, GetEndX() - GetStartX());

  label_1_->SetPosition(gfx::Point(GetStartX(), OffsetY(label_1_)));
  link_->SetPosition(gfx::Point(label_1_->bounds().right(), OffsetY(link_)));
  label_2_->SetPosition(gfx::Point(link_->bounds().right(), OffsetY(label_2_)));
}

int AlternateNavInfoBarView::GetContentMinimumWidth() const {
  if (base::FeatureList::IsEnabled(features::kInfobarRefresh)) {
    return 0;
  }
  int label_1_width = label_1_->GetMinimumSize().width();
  return label_1_width ? label_1_width : link_->GetMinimumSize().width();
}

AlternateNavInfoBarDelegate* AlternateNavInfoBarView::GetDelegate() {
  return static_cast<AlternateNavInfoBarDelegate*>(delegate());
}
