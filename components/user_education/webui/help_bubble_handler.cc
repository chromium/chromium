// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/help_bubble_handler.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom-shared.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

namespace user_education {

namespace {

// Converts help bubble arrow to WebUI bubble position. This is not a complete
// mapping as many HelpBubbleArrow options are not (yet) supported in WebUI.
help_bubble::mojom::HelpBubblePosition HelpBubbleArrowToPosition(
    HelpBubbleArrow arrow) {
  switch (arrow) {
    case HelpBubbleArrow::kBottomLeft:
    case HelpBubbleArrow::kBottomCenter:
    case HelpBubbleArrow::kBottomRight:
      return help_bubble::mojom::HelpBubblePosition::ABOVE;
    case HelpBubbleArrow::kLeftTop:
    case HelpBubbleArrow::kLeftCenter:
    case HelpBubbleArrow::kLeftBottom:
      return help_bubble::mojom::HelpBubblePosition::RIGHT;
    case HelpBubbleArrow::kRightTop:
    case HelpBubbleArrow::kRightCenter:
    case HelpBubbleArrow::kRightBottom:
      return help_bubble::mojom::HelpBubblePosition::LEFT;
    default:
      return help_bubble::mojom::HelpBubblePosition::BELOW;
  }
}

}  // namespace

struct HelpBubbleHandlerBase::ElementData {
  ElementData() = default;
  ~ElementData() = default;
  ElementData(ElementData&& other) = default;
  ElementData& operator=(ElementData&& other) = default;

  std::unique_ptr<TrackedElementWebUI> element;
  std::unique_ptr<HelpBubbleParams> params;
  base::raw_ptr<HelpBubbleWebUI> help_bubble = nullptr;

  // This is set to true if we are closing the help bubble as the result of a
  // message from the WebUI, rather than a browser-side event. It is used as a
  // guard to prevent a loop where we receive a message that the bubble is
  // closing and then tell the WebUI to close the bubble in response.
  bool closing = false;
};

HelpBubbleHandlerBase::HelpBubbleHandlerBase(
    std::unique_ptr<ClientProvider> client_provider,
    const std::vector<ui::ElementIdentifier>& identifiers,
    ui::ElementContext context)
    : client_provider_(std::move(client_provider)), context_(context) {
  DCHECK(context_);
  for (auto identifier : identifiers) {
    DCHECK(identifier);
    const auto it = element_data_.emplace(identifier, ElementData());
    DCHECK(it.second) << "Duplicate identifier not allowed: " << identifier;
    it.first->second.element =
        std::make_unique<TrackedElementWebUI>(this, identifier, context);
  }
}

HelpBubbleHandlerBase::~HelpBubbleHandlerBase() {
  for (auto& [id, data] : element_data_) {
    if (data.help_bubble)
      data.help_bubble->Close();
  }
}

help_bubble::mojom::HelpBubbleClient* HelpBubbleHandlerBase::GetClient() {
  return client_provider_->GetClient();
}

void HelpBubbleHandlerBase::ReportBadMessage(base::StringPiece error) {
  NOTREACHED() << error;
}

std::unique_ptr<HelpBubbleWebUI> HelpBubbleHandlerBase::CreateHelpBubble(
    ui::ElementIdentifier identifier,
    HelpBubbleParams params) {
  const auto it = element_data_.find(identifier);
  if (it == element_data_.end()) {
    NOTREACHED() << "Identifier " << identifier << " was never registered.";
    return nullptr;
  }

  auto& data = it->second;
  if (data.params) {
    NOTREACHED() << "A help bubble is already being shown for " << identifier;
    auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
    if (data.help_bubble) {
      data.help_bubble->Close();
      if (!weak_ptr)
        return nullptr;
    }
  }
  data.params = std::make_unique<HelpBubbleParams>(std::move(params));
  auto result = base::WrapUnique(new HelpBubbleWebUI(this, identifier));

  auto mojom_params = help_bubble::mojom::HelpBubbleParams::New();
  mojom_params->native_identifier = identifier.GetName();
  mojom_params->body_text = base::UTF16ToUTF8(data.params->body_text);
  mojom_params->close_button_alt_text =
      base::UTF16ToUTF8(data.params->close_button_alt_text);
  mojom_params->force_close_button = data.params->force_close_button;
  mojom_params->position = HelpBubbleArrowToPosition(data.params->arrow);
  if (data.params->progress) {
    mojom_params->progress = help_bubble::mojom::Progress::New();
    mojom_params->progress->current = data.params->progress->first;
    mojom_params->progress->total = data.params->progress->second;
  }
  if (!data.params->title_text.empty()) {
    mojom_params->title_text = base::UTF16ToUTF8(data.params->title_text);
  }
  for (auto& button : data.params->buttons) {
    auto mojom_button = help_bubble::mojom::HelpBubbleButtonParams::New();
    mojom_button->text = base::UTF16ToUTF8(button.text);
    mojom_button->is_default = button.is_default;
    mojom_params->buttons.emplace_back(std::move(mojom_button));
  }

  GetClient()->ShowHelpBubble(std::move(mojom_params));
  it->second.help_bubble = result.get();
  return result;
}

void HelpBubbleHandlerBase::OnHelpBubbleClosing(
    ui::ElementIdentifier anchor_id) {
  const auto it = element_data_.find(anchor_id);
  if (it == element_data_.end()) {
    NOTREACHED() << "Identifier " << anchor_id << " was never registered.";
    return;
  }
  if (!it->second.closing)
    GetClient()->HideHelpBubble(anchor_id.GetName());
  it->second.help_bubble = nullptr;
  it->second.params.reset();
}

void HelpBubbleHandlerBase::HelpBubbleAnchorVisibilityChanged(
    const std::string& identifier_name,
    bool visible) {
  ui::ElementIdentifier id;
  ElementData* const data = GetDataByName(identifier_name, &id);
  if (!data)
    return;

  // Note: any of the following calls could destroy *this* via a callback.
  if (!data->element->visible() && visible) {
    data->element->SetVisible(true);
  } else if (data->element->visible() && !visible) {
    // Is a help bubble currently showing?
    if (data->params) {
      // Currently, this is the only call that could trigger callbacks and which
      // has additional code which executes after it. If that changes, the weak
      // pointer can be moved closer to the top of this method.
      auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
      HelpBubbleClosed(identifier_name, false);
      if (!weak_ptr)
        return;
    }
    data->element->SetVisible(false);
  }
}

void HelpBubbleHandlerBase::HelpBubbleButtonPressed(
    const std::string& identifier_name,
    uint8_t button_index) {
  ElementData* const data = GetDataByName(identifier_name);
  if (!data)
    return;
  if (!data->params) {
    ReportBadMessage(
        "HelpBubbleButtonPressed message received for anchor element \"%s\" "
        "but no help bubble was open.");
    return;
  }
  if (button_index >= data->params->buttons.size()) {
    ReportBadMessage(base::StringPrintf(
        "HelpBubbleButtonPressed received but button index was invalid; "
        "got %u but there are only %zu buttons.",
        button_index, data->params->buttons.size()));
    return;
  }

  // We can never ensure that objects will persist across callbacks.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  data->closing = true;

  base::OnceClosure callback =
      std::move(data->params->buttons[button_index].callback);
  if (callback)
    std::move(callback).Run();

  if (!weak_ptr)
    return;

  if (data->help_bubble)
    data->help_bubble->Close();

  if (!weak_ptr)
    return;

  data->closing = false;
}

void HelpBubbleHandlerBase::HelpBubbleClosed(const std::string& identifier_name,
                                             bool by_user) {
  ElementData* const data = GetDataByName(identifier_name);
  if (!data)
    return;
  if (!data->params) {
    ReportBadMessage(base::StringPrintf(
        "HelpBubbleClosed message received for identifier_name = \"%s\" but no "
        "help bubble was open.",
        identifier_name.c_str()));
    return;
  }

  // We can never ensure that `this` will persist across callbacks.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  data->closing = true;

  if (by_user) {
    base::OnceClosure callback = std::move(data->params->dismiss_callback);
    if (callback) {
      std::move(callback).Run();
      if (!weak_ptr)
        return;
    }
  }

  // This could also theoretically trigger callbacks.
  data->help_bubble->Close();

  if (!weak_ptr)
    return;

  data->closing = false;
}

bool HelpBubbleHandlerBase::ToggleHelpBubbleFocusForAccessibility(
    ui::ElementIdentifier anchor_id) {
  if (base::Contains(element_data_, anchor_id)) {
    GetClient()->ToggleFocusForAccessibility(anchor_id.GetName());
    return true;
  }
  return false;
}

gfx::Rect HelpBubbleHandlerBase::GetHelpBubbleBoundsInScreen(
    ui::ElementIdentifier anchor_id) const {
  // TODO(dfried): implement.
  return gfx::Rect();
}

HelpBubbleHandlerBase::ElementData* HelpBubbleHandlerBase::GetDataByName(
    const std::string& identifier_name,
    ui::ElementIdentifier* found_identifier) {
  for (auto& [id, data] : element_data_) {
    if (id.GetName() == identifier_name) {
      if (found_identifier)
        *found_identifier = id;
      return &data;
    }
  }
  if (found_identifier)
    *found_identifier = ui::ElementIdentifier();
  ReportBadMessage(base::StringPrintf(
      "HelpBubbleHandler IPC message received with unrecognized "
      "identifier_name: \"%s\"",
      identifier_name.c_str()));
  return nullptr;
}

HelpBubbleHandler::ClientProvider::ClientProvider(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client)
    : remote_client_(std::move(pending_client)) {}

HelpBubbleHandler::ClientProvider::~ClientProvider() = default;

help_bubble::mojom::HelpBubbleClient*
HelpBubbleHandler::ClientProvider::GetClient() {
  return remote_client_.get();
}

HelpBubbleHandler::HelpBubbleHandler(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler>
        pending_handler,
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client,
    content::WebContents* web_contents,
    const std::vector<ui::ElementIdentifier>& identifiers)
    : HelpBubbleHandlerBase(
          std::make_unique<ClientProvider>(std::move(pending_client)),
          identifiers,
          ui::ElementContext(web_contents)),
      receiver_(this, std::move(pending_handler)) {}

HelpBubbleHandler::~HelpBubbleHandler() = default;

void HelpBubbleHandler::ReportBadMessage(base::StringPiece error) {
  receiver_.ReportBadMessage(std::move(error));
}

}  // namespace user_education
