// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_DIALOG_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/metadata/view_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kProtocolHandlerPickerDialogRememberSelectionCheckboxId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kProtocolHandlerPickerDialogOkButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kProtocolHandlerPickerDialogSelectionId);

namespace web_app {

struct ProtocolHandlerPickerDialogEntry {
  std::string app_id;
  std::u16string app_name;
  ui::ImageModel icon;
};

using ProtocolHandlerPickerDialogEntries =
    std::vector<ProtocolHandlerPickerDialogEntry>;

// Represents a single row within the scrollable selection.
// <app-icon> <app-name> <optional-check-icon-when-selected>
class ProtocolHandlerPickerSelectionRowView : public views::Button {
  METADATA_HEADER(ProtocolHandlerPickerSelectionRowView, views::Button)

 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnRowClicked(ProtocolHandlerPickerSelectionRowView*) = 0;
  };

  // When `include_check_icon` is false, no check icon is displayed when
  // selected.
  ProtocolHandlerPickerSelectionRowView(
      const ProtocolHandlerPickerDialogEntry& app,
      Delegate& delegate,
      bool include_check_icon = true);
  ~ProtocolHandlerPickerSelectionRowView() override;

  void SetSelected(bool selected);
  bool IsSelected() const;

  const std::string& app_id() const { return app_id_; }

  // For the ViewBuilder.
  void SetCheckedState(ax::mojom::CheckedState state);

 private:
  void OnRowClicked();

  // views::Button:
  void StateChanged(ButtonState old_state) override;

  void UpdateBackground();

  const std::string app_id_;
  bool is_selected_ = false;
  raw_ptr<views::ImageView> check_icon_ = nullptr;

  const raw_ref<Delegate> delegate_;
};

// Represents the selection of potential protocol handlers; it becomes
// scrollable when there are >3 entries. At most one entry can be in the
// selected state at a time.
BEGIN_VIEW_BUILDER(, ProtocolHandlerPickerSelectionRowView, views::Button)
VIEW_BUILDER_METHOD(SetCheckedState, ax::mojom::CheckedState)
END_VIEW_BUILDER

// The callback to be run when the dialog is accepted.
using OnPreferredHandlerSelected =
    base::OnceCallback<void(const std::string& app_id, bool remember_choice)>;

std::unique_ptr<ui::DialogModel> CreateProtocolHandlerPickerDialog(
    const GURL& protocol_url,
    const ProtocolHandlerPickerDialogEntries& apps,
    const std::optional<std::u16string>& initiator_display_name,
    OnPreferredHandlerSelected callback);

}  // namespace web_app

DEFINE_VIEW_BUILDER(, web_app::ProtocolHandlerPickerSelectionRowView)

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_DIALOG_H_
