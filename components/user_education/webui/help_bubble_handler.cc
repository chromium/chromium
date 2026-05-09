// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/help_bubble_handler.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/pass_key.h"
#include "components/user_education/common/help_bubble/help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace user_education {

namespace {

// Converts help bubble arrow to WebUI bubble position. This is not a complete
// mapping as many HelpBubbleArrow options are not (yet) supported in WebUI.
help_bubble::mojom::HelpBubbleArrowPosition HelpBubbleArrowToPosition(
    HelpBubbleArrow arrow) {
  switch (arrow) {
    case HelpBubbleArrow::kBottomLeft:
      return help_bubble::mojom::HelpBubbleArrowPosition::BOTTOM_LEFT;
    case HelpBubbleArrow::kBottomCenter:
      return help_bubble::mojom::HelpBubbleArrowPosition::BOTTOM_CENTER;
    case HelpBubbleArrow::kBottomRight:
      return help_bubble::mojom::HelpBubbleArrowPosition::BOTTOM_RIGHT;

    case HelpBubbleArrow::kTopLeft:
      return help_bubble::mojom::HelpBubbleArrowPosition::TOP_LEFT;
    case HelpBubbleArrow::kTopCenter:
      return help_bubble::mojom::HelpBubbleArrowPosition::TOP_CENTER;
    case HelpBubbleArrow::kTopRight:
      return help_bubble::mojom::HelpBubbleArrowPosition::TOP_RIGHT;

    case HelpBubbleArrow::kLeftTop:
      return help_bubble::mojom::HelpBubbleArrowPosition::LEFT_TOP;
    case HelpBubbleArrow::kLeftCenter:
      return help_bubble::mojom::HelpBubbleArrowPosition::LEFT_CENTER;
    case HelpBubbleArrow::kLeftBottom:
      return help_bubble::mojom::HelpBubbleArrowPosition::LEFT_BOTTOM;

    case HelpBubbleArrow::kRightTop:
      return help_bubble::mojom::HelpBubbleArrowPosition::RIGHT_TOP;
    case HelpBubbleArrow::kRightCenter:
      return help_bubble::mojom::HelpBubbleArrowPosition::RIGHT_CENTER;
    case HelpBubbleArrow::kRightBottom:
      return help_bubble::mojom::HelpBubbleArrowPosition::RIGHT_BOTTOM;

    default:
      NOTIMPLEMENTED();
  }
  return help_bubble::mojom::HelpBubbleArrowPosition::TOP_CENTER;
}

std::string SnakeCaseFromCamelCase(std::string input) {
  std::string output;
  output.reserve(input.size());
  for (const char c : input) {
    if (absl::ascii_isupper(static_cast<unsigned char>(c)) && !output.empty()) {
      output.push_back('_');
    }
    output.push_back(absl::ascii_tolower(static_cast<unsigned char>(c)));
  }
  return output;
}

// Retrieve the file name from the generated gfx::VectorIcon name
// - Remove the 'k' prefix and 'Icon' suffix from gfx::VectorIcon.name
// - The remaining portion of the name is converted from CamelCase to
//      snake_case to yield the original file name
std::string GetFileNameFromIcon(const gfx::VectorIcon* icon) {
  std::string icon_name = icon->name;
  constexpr char kPrefix[] = "k";
  constexpr char kSuffix[] = "Icon";
  DCHECK(base::StartsWith(icon_name, kPrefix));
  DCHECK(base::EndsWith(icon_name, kSuffix));
  icon_name.erase(0, strlen(kPrefix));
  icon_name.erase(icon_name.length() - strlen(kSuffix));
  return SnakeCaseFromCamelCase(icon_name);
}

content::WebContents* GetWebContentsCallbackForWebUIController(
    content::WebUIController* controller) {
  // A WebContents is always associated with a WebUIController, so this never
  // returns nullptr in production code. The only possible reason for
  // returning nullptr is in unit tests where the test WebUIController
  // implementation is not set up correctly. If that happens, the test support
  // code should be fixed.
  return controller->web_ui()->GetWebContents();
}

}  // namespace

struct HelpBubbleHandlerBase::ElementData {
  ElementData() = default;
  ElementData(ElementData&& other) noexcept = default;
  ElementData& operator=(ElementData&& other) noexcept = default;
  ~ElementData() = default;

  bool has_webui_help_bubble() const { return static_cast<bool>(params); }

  std::unique_ptr<HelpBubbleParams> params;
  raw_ptr<HelpBubbleWebUI> help_bubble = nullptr;
  std::unique_ptr<ui::TrackedElementVisibilityLock> visibility_lock;
  base::CallbackListSubscription external_bubble_subscription;
  base::CallbackListSubscription anchor_hidden_subscription;

  // This is set to true if we are closing the help bubble as the result of a
  // message from the WebUI, rather than a browser-side event. It is used as a
  // guard to prevent a loop where we receive a message that the bubble is
  // closing and then tell the WebUI to close the bubble in response.
  bool closing = false;
};

HelpBubbleHandlerBase::HelpBubbleHandlerBase(
    std::unique_ptr<ClientProvider> client_provider,
    GetWebContentsCallback get_web_contents_callback,
    const std::vector<ui::ElementIdentifier>& identifiers,
    ui::ElementContext context)
    : client_provider_(std::move(client_provider)),
      get_web_contents_callback_(std::move(get_web_contents_callback)),
      context_(context) {
  DCHECK(client_provider_);
  DCHECK(get_web_contents_callback_);
  DCHECK(context_);

  for (auto identifier : identifiers) {
    DCHECK(identifier);
    const auto it = element_data_.emplace(identifier, ElementData());
    DCHECK(it.second) << "Duplicate identifier not allowed: " << identifier;
  }
}

HelpBubbleHandlerBase::~HelpBubbleHandlerBase() {
  for (auto& [id, data] : element_data_) {
    if (data.help_bubble) {
      data.help_bubble->Close(HelpBubble::CloseReason::kBubbleDestroyed);
    }
  }
}

content::WebContents* HelpBubbleHandlerBase::GetWebContents() {
  return get_web_contents_callback_.Run();
}

bool HelpBubbleHandlerBase::IsHelpBubbleShowingForTesting(
    ui::ElementIdentifier id) const {
  const auto it = element_data_.find(id);
  return it != element_data_.end() && it->second.has_webui_help_bubble();
}

help_bubble::mojom::HelpBubbleClient* HelpBubbleHandlerBase::GetClient() {
  return client_provider_->GetClient();
}

void HelpBubbleHandlerBase::ReportBadMessage(std::string_view error) {
  NOTREACHED() << error;
}

std::unique_ptr<HelpBubbleWebUI> HelpBubbleHandlerBase::CreateHelpBubble(
    ui::ElementIdentifier identifier,
    HelpBubbleParams params) {
  const auto it = element_data_.find(identifier);
  if (it == element_data_.end()) {
    NOTREACHED() << "Identifier " << identifier << " was never registered.";
  }

  auto& data = it->second;
  if (data.has_webui_help_bubble()) {
    LOG(WARNING) << "A help bubble is already being shown for " << identifier;
    auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
    if (data.help_bubble) {
      data.help_bubble->Close(HelpBubble::CloseReason::kProgrammaticallyClosed);
      if (!weak_ptr) {
        return nullptr;
      }
    }
  }
  data.params = std::make_unique<HelpBubbleParams>(std::move(params));
  if (tracked_element_handler_) {
    data.visibility_lock =
        tracked_element_handler_->LockVisible(identifier.GetName());
  }
  data.anchor_hidden_subscription =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          identifier, context_,
          base::BindRepeating(
              [](base::WeakPtr<HelpBubbleHandlerBase> handler,
                 ui::ElementIdentifier id, ui::TrackedElement* el) {
                if (handler) {
                  const auto it = handler->element_data_.find(id);
                  if (it != handler->element_data_.end() &&
                      it->second.help_bubble) {
                    it->second.help_bubble->Close(
                        HelpBubble::CloseReason::kAnchorHidden);
                  }
                }
              },
              weak_ptr_factory_.GetWeakPtr(), identifier));
  auto result = base::WrapUnique(new HelpBubbleWebUI(this, identifier));
  data.help_bubble = result.get();

  auto mojom_params = help_bubble::mojom::HelpBubbleParams::New();
  mojom_params->native_identifier = identifier.GetName();
  mojom_params->body_text = base::UTF16ToUTF8(data.params->body_text);
  mojom_params->close_button_alt_text =
      base::UTF16ToUTF8(data.params->close_button_alt_text);
  auto timeout = data.params->timeout.value_or(
      data.params->buttons.empty() ? kDefaultTimeoutWithoutButtons
                                   : kDefaultTimeoutWithButtons);
  if (!timeout.is_zero())
    mojom_params->timeout = timeout;
  if (data.params->body_icon)
    mojom_params->body_icon_name = GetFileNameFromIcon(data.params->body_icon);
  mojom_params->body_icon_alt_text =
      base::UTF16ToUTF8(data.params->body_icon_alt_text);
  mojom_params->focus_on_show_hint = data.params->focus_on_show_hint;
  mojom_params->position = HelpBubbleArrowToPosition(data.params->arrow);
  if (data.params->progress) {
    mojom_params->progress = help_bubble::mojom::Progress::New();
    mojom_params->progress->current = data.params->progress->first;
    mojom_params->progress->total = data.params->progress->second;
  }
  if (!data.params->title_text.empty())
    mojom_params->title_text = base::UTF16ToUTF8(data.params->title_text);
  for (auto& button : data.params->buttons) {
    auto mojom_button = help_bubble::mojom::HelpBubbleButtonParams::New();
    mojom_button->text = base::UTF16ToUTF8(button.text);
    mojom_button->is_default = button.is_default;
    mojom_params->buttons.emplace_back(std::move(mojom_button));
  }

  GetClient()->ShowHelpBubble(std::move(mojom_params));
  return result;
}

void HelpBubbleHandlerBase::OnHelpBubbleClosing(
    ui::ElementIdentifier anchor_id) {
  const auto it = element_data_.find(anchor_id);
  if (it == element_data_.end()) {
    NOTREACHED() << "Identifier " << anchor_id << " was never registered.";
  }
  if (!it->second.closing) {
    GetClient()->HideHelpBubble(anchor_id.GetName());
  }
  it->second.help_bubble = nullptr;
  it->second.params.reset();
  it->second.visibility_lock.reset();
  it->second.anchor_hidden_subscription = base::CallbackListSubscription();
}

void HelpBubbleHandlerBase::HelpBubbleButtonPressed(
    const std::string& identifier_name,
    uint8_t button_index) {
  ElementData* const data = GetDataByName(identifier_name);
  if (!data)
    return;
  if (!data->has_webui_help_bubble()) {
    ReportBadMessage(
        base::StringPrintf("HelpBubbleButtonPressed message received for "
                           "anchor element \"%s\" but no help bubble was open.",
                           identifier_name.c_str()));
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

  if (data->help_bubble) {
    data->help_bubble->Close(HelpBubble::CloseReason::kProgrammaticallyClosed);
  }

  if (!weak_ptr) {
    return;
  }

  data->closing = false;
}

void HelpBubbleHandlerBase::HelpBubbleClosed(
    const std::string& identifier_name,
    help_bubble::mojom::HelpBubbleClosedReason reason) {
  ElementData* const data = GetDataByName(identifier_name);
  if (!data)
    return;
  if (!data->has_webui_help_bubble()) {
    ReportBadMessage(base::StringPrintf(
        "HelpBubbleClosed message received for identifier_name = \"%s\" but no "
        "help bubble was open.",
        identifier_name.c_str()));
    return;
  }

  // We can never ensure that `this` will persist across callbacks.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  data->closing = true;

  base::OnceClosure callback;
  switch (reason) {
    case help_bubble::mojom::HelpBubbleClosedReason::kDismissedByUser:
      callback = std::move(data->params->dismiss_callback);
      break;
    case help_bubble::mojom::HelpBubbleClosedReason::kTimedOut:
      callback = std::move(data->params->timeout_callback);
      break;
    case help_bubble::mojom::HelpBubbleClosedReason::kPageChanged:
      break;
  }

  if (callback) {
    std::move(callback).Run();
    if (!weak_ptr)
      return;
  }

  // This could also theoretically trigger callbacks.
  if (data->help_bubble) {
    data->help_bubble->Close(HelpBubble::CloseReason::kProgrammaticallyClosed);
  }

  if (!weak_ptr)
    return;

  data->closing = false;
}

void HelpBubbleHandlerBase::BindTrackedElementHandler(
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        handler) {
  std::vector<ui::ElementIdentifier> identifiers;
  identifiers.reserve(element_data_.size());
  for (const auto& [id, _] : element_data_) {
    identifiers.emplace_back(id);
  }
  tracked_element_handler_ = std::make_unique<ui::TrackedElementHandler>(
      GetWebContents(), std::move(handler), context_, identifiers);
  tracked_element_handler_->set_help_bubble_helper(this);
}

bool HelpBubbleHandlerBase::ToggleHelpBubbleFocusForAccessibility(
    ui::ElementIdentifier anchor_id) {
  if (element_data_.contains(anchor_id)) {
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

void HelpBubbleHandlerBase::OnFloatingHelpBubbleCreated(
    ui::ElementIdentifier anchor_id,
    HelpBubble* help_bubble) {
  GetClient()->ExternalHelpBubbleUpdated(anchor_id.GetName(), true);
  const auto it = element_data_.find(anchor_id);
  if (it == element_data_.end()) {
    return;
  }
  DCHECK(!it->second.external_bubble_subscription);
  if (tracked_element_handler_) {
    it->second.visibility_lock =
        tracked_element_handler_->LockVisible(anchor_id.GetName());
  }
  it->second.external_bubble_subscription = help_bubble->AddOnClosingCallback(
      base::BindOnce(&HelpBubbleHandlerBase::OnFloatingHelpBubbleClosed,
                     weak_ptr_factory_.GetWeakPtr(), anchor_id));
}

void HelpBubbleHandlerBase::OnFloatingHelpBubbleClosed(
    ui::ElementIdentifier anchor_id,
    const HelpBubble* help_bubble,
    HelpBubble::CloseReason) {
  const auto it = element_data_.find(anchor_id);
  if (it == element_data_.end()) {
    return;
  }
  it->second.external_bubble_subscription = base::CallbackListSubscription();
  it->second.visibility_lock.reset();
  GetClient()->ExternalHelpBubbleUpdated(anchor_id.GetName(), false);
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

class HelpBubbleHandler::ClientProvider
    : public HelpBubbleHandlerBase::ClientProvider {
 public:
  explicit ClientProvider(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client)
      : remote_client_(std::move(pending_client)) {}
  ~ClientProvider() override = default;

  help_bubble::mojom::HelpBubbleClient* GetClient() override {
    return remote_client_.get();
  }

 private:
  mojo::Remote<help_bubble::mojom::HelpBubbleClient> remote_client_;
};

HelpBubbleHandler::HelpBubbleHandler(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler>
        pending_handler,
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client,
    content::WebUIController* controller,
    const std::vector<ui::ElementIdentifier>& identifiers)
    : HelpBubbleHandler(
          std::move(pending_handler),
          std::move(pending_client),
          base::BindRepeating(&GetWebContentsCallbackForWebUIController,
                              controller),
          controller,
          identifiers) {
  DCHECK(controller);
}

HelpBubbleHandler::HelpBubbleHandler(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler>
        pending_handler,
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client,
    GetWebContentsCallback get_web_contents_callback,
    void* context,
    const std::vector<ui::ElementIdentifier>& identifiers)
    : HelpBubbleHandlerBase(
          std::make_unique<ClientProvider>(std::move(pending_client)),
          std::move(get_web_contents_callback),
          identifiers,
          ui::ElementContext(context, base::PassKey<HelpBubbleHandler>())),
      receiver_(this, std::move(pending_handler)) {}

HelpBubbleHandler::~HelpBubbleHandler() = default;

void HelpBubbleHandler::ReportBadMessage(std::string_view error) {
  receiver_.ReportBadMessage(std::move(error));
}

}  // namespace user_education
