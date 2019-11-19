// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/remove_suggestion_bubble.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

class RemoveSuggestionBubbleDialogDelegateView
    : public views::BubbleDialogDelegateView {
 public:
  RemoveSuggestionBubbleDialogDelegateView(
      TemplateURLService* template_url_service,
      views::View* anchor_view,
      const AutocompleteMatch& match,
      base::OnceClosure remove_closure)
      : views::BubbleDialogDelegateView(anchor_view,
                                        views::BubbleBorder::TOP_RIGHT),
        match_(match),
        remove_closure_(std::move(remove_closure)) {
    DCHECK(template_url_service);
    DCHECK(match_.SupportsDeletion());

    DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                     l10n_util::GetStringUTF16(IDS_REMOVE));
    DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                     l10n_util::GetStringUTF16(IDS_CANCEL));

    auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    layout_manager->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    // TODO(tommycli): Replace this with the real spacing from UX.
    layout_manager->set_between_child_spacing(16);

    // Get the Search Provider name associated with this match.
    base::string16 search_provider_short_name;
    const TemplateURL* template_url =
        match.GetTemplateURL(template_url_service, false);
    // If the match has no associated Search Provider, get the default one,
    // although this may still fail if it's forbidden by policy.
    if (!template_url) {
      template_url = template_url_service->GetDefaultSearchProvider();
    }
    if (template_url) {
      search_provider_short_name =
          template_url->AdjustedShortNameForLocaleDirection();
    }

    views::Label* description_label =
        new views::Label(l10n_util::GetStringFUTF16(
            IDS_OMNIBOX_REMOVE_SUGGESTION_BUBBLE_DESCRIPTION,
            search_provider_short_name));
    description_label->SetMultiLine(true);
    description_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    AddChildView(description_label);

    // TODO(tommycli): Indent and set a smaller font per UX suggestions.
    views::Label* url_label = new views::Label(match.contents);
    url_label->SetMultiLine(true);
    url_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    AddChildView(url_label);
  }

  // views::DialogDelegateView:
  int GetDialogButtons() const override {
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  }
  bool Accept() override {
    std::move(remove_closure_).Run();
    return true;
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    // TODO(tommycli): Replace with the real width from UX.
    return gfx::Size(500, GetHeightForWidth(500));
  }

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_WINDOW; }
  base::string16 GetWindowTitle() const override {
    return l10n_util::GetStringUTF16(
        IDS_OMNIBOX_REMOVE_SUGGESTION_BUBBLE_TITLE);
  }

 private:
  AutocompleteMatch match_;
  base::OnceClosure remove_closure_;
};

}  // namespace

void ShowRemoveSuggestion(TemplateURLService* template_url_service,
                          views::View* anchor_view,
                          const AutocompleteMatch& match,
                          base::OnceClosure remove_closure) {
  views::BubbleDialogDelegateView::CreateBubble(
      new RemoveSuggestionBubbleDialogDelegateView(
          template_url_service, anchor_view, match, std::move(remove_closure)))
      ->Show();
}
