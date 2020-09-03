// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_service.h"

#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"
#include "components/arc/ime/arc_ime_bridge_impl.h"
#include "components/arc/ime/arc_ime_util.h"
#include "components/arc/ime/key_event_result_receiver.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/range/range.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/ime_util_chromeos.h"

namespace arc {

namespace {

base::Optional<double> g_override_default_device_scale_factor;

double GetDefaultDeviceScaleFactor() {
  if (g_override_default_device_scale_factor.has_value())
    return g_override_default_device_scale_factor.value();
  if (!exo::WMHelper::HasInstance())
    return 1.0;
  return exo::WMHelper::GetInstance()->GetDefaultDeviceScaleFactor();
}

class ArcWindowDelegateImpl : public ArcImeService::ArcWindowDelegate {
 public:
  explicit ArcWindowDelegateImpl(ArcImeService* ime_service)
      : ime_service_(ime_service) {}

  ~ArcWindowDelegateImpl() override = default;

  bool IsInArcAppWindow(const aura::Window* window) const override {
    // WMHelper is not craeted in browser_tests.
    if (!exo::WMHelper::HasInstance())
      return false;
    aura::Window* active = exo::WMHelper::GetInstance()->GetActiveWindow();
    for (; window; window = window->parent()) {
      if (IsArcAppWindow(window))
        return true;

      // IsArcAppWindow returns false for a window of ARC++ Kiosk app, so we
      // have to check application id of the active window to cover that case.
      // TODO(yhanada): Make IsArcAppWindow support a window of ARC++ Kiosk.
      // Specifically, a window of ARC++ Kiosk should have ash::AppType::ARC_APP
      // property. Please see implementation of IsArcAppWindow().
      if (window == active && IsArcKioskMode() &&
          GetWindowTaskId(window) != kNoTaskId) {
        return true;
      }
    }
    return false;
  }

  void RegisterFocusObserver() override {
    // WMHelper is not craeted in browser_tests.
    if (!exo::WMHelper::HasInstance())
      return;
    exo::WMHelper::GetInstance()->AddFocusObserver(ime_service_);
  }

  void UnregisterFocusObserver() override {
    // If WMHelper is already destroyed, do nothing.
    // TODO(crbug.com/748380): Fix shutdown order.
    if (!exo::WMHelper::HasInstance())
      return;
    exo::WMHelper::GetInstance()->RemoveFocusObserver(ime_service_);
  }

  ui::InputMethod* GetInputMethodForWindow(
      aura::Window* window) const override {
    if (!window || !window->GetHost())
      return nullptr;
    return window->GetHost()->GetInputMethod();
  }

  bool IsImeBlocked(aura::Window* window) const override {
    // WMHelper is not craeted in browser_tests.
    if (!exo::WMHelper::HasInstance())
      return false;
    return exo::WMHelper::GetInstance()->IsImeBlocked(window);
  }

 private:
  ArcImeService* const ime_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcWindowDelegateImpl);
};

// Singleton factory for ArcImeService.
class ArcImeServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcImeService,
          ArcImeServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcImeServiceFactory";

  static ArcImeServiceFactory* GetInstance() {
    return base::Singleton<ArcImeServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcImeServiceFactory>;
  ArcImeServiceFactory() = default;
  ~ArcImeServiceFactory() override = default;
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// ArcImeService main implementation:

// static
ArcImeService* ArcImeService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcImeServiceFactory::GetForBrowserContext(context);
}

ArcImeService::ArcImeService(content::BrowserContext* context,
                             ArcBridgeService* bridge_service)
    : ArcImeService(context,
                    bridge_service,
                    std::make_unique<ArcWindowDelegateImpl>(this)) {}

ArcImeService::ArcImeService(content::BrowserContext* context,
                             ArcBridgeService* bridge_service,
                             std::unique_ptr<ArcWindowDelegate> delegate)
    : ime_bridge_(new ArcImeBridgeImpl(this, bridge_service)),
      arc_window_delegate_(std::move(delegate)),
      ime_type_(ui::TEXT_INPUT_TYPE_NONE),
      ime_flags_(ui::TEXT_INPUT_FLAG_NONE),
      is_personalized_learning_allowed_(false),
      has_composition_text_(false),
      receiver_(std::make_unique<KeyEventResultReceiver>()) {
  if (aura::Env::HasInstance())
    aura::Env::GetInstance()->AddObserver(this);
  arc_window_delegate_->RegisterFocusObserver();
}

ArcImeService::~ArcImeService() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->DetachTextInputClient(this);

  if (focused_arc_window_)
    focused_arc_window_->RemoveObserver(this);
  arc_window_delegate_->UnregisterFocusObserver();
  if (aura::Env::HasInstance())
    aura::Env::GetInstance()->RemoveObserver(this);

  // KeyboardController is destroyed before ArcImeService (except in tests),
  // so check whether there is a KeyboardController first before removing |this|
  // from KeyboardController observers.
  if (keyboard::KeyboardUIController::HasInstance()) {
    auto* keyboard_controller = keyboard::KeyboardUIController::Get();
    if (keyboard_controller->HasObserver(this))
      keyboard_controller->RemoveObserver(this);
  }
}

void ArcImeService::SetImeBridgeForTesting(
    std::unique_ptr<ArcImeBridge> test_ime_bridge) {
  ime_bridge_ = std::move(test_ime_bridge);
}

ui::InputMethod* ArcImeService::GetInputMethod() {
  return arc_window_delegate_->GetInputMethodForWindow(focused_arc_window_);
}

void ArcImeService::ReattachInputMethod(aura::Window* old_window,
                                        aura::Window* new_window) {
  ui::InputMethod* const old_ime =
      arc_window_delegate_->GetInputMethodForWindow(old_window);
  ui::InputMethod* const new_ime =
      arc_window_delegate_->GetInputMethodForWindow(new_window);

  if (old_ime != new_ime) {
    if (old_ime)
      old_ime->DetachTextInputClient(this);
    if (new_ime)
      new_ime->SetFocusedTextInputClient(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::EnvObserver:

void ArcImeService::OnWindowInitialized(aura::Window* new_window) {
  if (keyboard::KeyboardUIController::HasInstance()) {
    auto* keyboard_controller = keyboard::KeyboardUIController::Get();
    if (keyboard_controller->IsEnabled() &&
        !keyboard_controller->HasObserver(this)) {
      keyboard_controller->AddObserver(this);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::WindowObserver:

void ArcImeService::OnWindowDestroying(aura::Window* window) {
  // This shouldn't be reached on production, since the window lost the focus
  // and called OnWindowFocused() before destroying.
  // But we handle this case for testing.
  if (window == focused_arc_window_)
    OnWindowFocused(nullptr, focused_arc_window_);
}

void ArcImeService::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                   aura::Window* new_root) {
  // IMEs are associated with root windows, hence we may need to detach/attach.
  if (window == focused_arc_window_)
    ReattachInputMethod(focused_arc_window_, new_root);
}

void ArcImeService::OnWindowPropertyChanged(aura::Window* window,
                                            const void* key,
                                            intptr_t old) {
  if (window == focused_arc_window_)
    return;

  bool ime_blocked = arc_window_delegate_->IsImeBlocked(focused_arc_window_);
  if (last_ime_blocked_ == ime_blocked)
    return;
  last_ime_blocked_ = ime_blocked;

  // IME blocking has changed.
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method) {
    if (has_composition_text_) {
      // If it has composition text, clear both ARC's current composition text
      // and Chrome IME's one.
      ClearCompositionText();
      input_method->CancelComposition(this);
    }
    input_method->OnTextInputTypeChanged(this);
  }
}

void ArcImeService::OnWindowRemoved(aura::Window* removed_window) {
  // |this| can lose the IME focus because |focused_arc_window_| may have
  // children other than ExoSurface e.g. WebContentsViewAura for CustomTabs.
  // Restore the IME focus when such a window is removed.
  ReattachInputMethod(nullptr, focused_arc_window_);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from exo::WMHelper::FocusChangeObserver:

void ArcImeService::OnWindowFocused(aura::Window* gained_focus,
                                    aura::Window* lost_focus) {
  if (lost_focus == gained_focus)
    return;

  const bool detach = (lost_focus && focused_arc_window_ == lost_focus);
  const bool attach = arc_window_delegate_->IsInArcAppWindow(gained_focus);

  if (detach) {
    // The focused window and the toplevel window are different in production,
    // but in tests they can be the same, so avoid adding the observer twice.
    if (focused_arc_window_ != focused_arc_window_->GetToplevelWindow())
      focused_arc_window_->GetToplevelWindow()->RemoveObserver(this);
    focused_arc_window_->RemoveObserver(this);
    focused_arc_window_ = nullptr;
  }
  if (attach) {
    DCHECK_EQ(nullptr, focused_arc_window_);
    focused_arc_window_ = gained_focus;
    focused_arc_window_->AddObserver(this);
    // The focused window and the toplevel window are different in production,
    // but in tests they can be the same, so avoid adding the observer twice.
    if (focused_arc_window_ != focused_arc_window_->GetToplevelWindow())
      focused_arc_window_->GetToplevelWindow()->AddObserver(this);
  }

  ReattachInputMethod(detach ? lost_focus : nullptr, focused_arc_window_);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from arc::ArcImeBridge::Delegate

void ArcImeService::OnTextInputTypeChanged(
    ui::TextInputType type,
    bool is_personalized_learning_allowed,
    int flags) {
  if (!ShouldSendUpdateToInputMethod())
    return;

  if (ime_type_ == type &&
      is_personalized_learning_allowed_ == is_personalized_learning_allowed &&
      ime_flags_ == flags) {
    return;
  }
  ime_type_ = type;
  is_personalized_learning_allowed_ = is_personalized_learning_allowed;
  ime_flags_ = flags;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnTextInputTypeChanged(this);

  // Call HideKeyboard() here. On a text field on an ARC++ app, just having
  // non-null text input type doesn't mean the virtual keyboard is necessary. If
  // the virtual keyboard is really needed, ShowVirtualKeyboardIfEnabled will be
  // called later.
  if (keyboard::KeyboardUIController::HasInstance()) {
    auto* keyboard_controller = keyboard::KeyboardUIController::Get();
    if (keyboard_controller->IsEnabled())
      keyboard_controller->HideKeyboardImplicitlyBySystem();
  }
}

void ArcImeService::OnCursorRectChanged(const gfx::Rect& rect,
                                        bool is_screen_coordinates) {
  if (!ShouldSendUpdateToInputMethod())
    return;

  InvalidateSurroundingTextAndSelectionRange();
  if (!UpdateCursorRect(rect, is_screen_coordinates))
    return;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnCaretBoundsChanged(this);
}

void ArcImeService::OnCancelComposition() {
  if (!ShouldSendUpdateToInputMethod())
    return;

  InvalidateSurroundingTextAndSelectionRange();
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->CancelComposition(this);
}

void ArcImeService::ShowVirtualKeyboardIfEnabled() {
  if (!ShouldSendUpdateToInputMethod())
    return;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method && input_method->GetTextInputClient() == this) {
    input_method->ShowVirtualKeyboardIfEnabled();
  }
}

void ArcImeService::OnCursorRectChangedWithSurroundingText(
    const gfx::Rect& rect,
    const gfx::Range& text_range,
    const base::string16& text_in_range,
    const gfx::Range& selection_range,
    bool is_screen_coordinates) {
  if (!ShouldSendUpdateToInputMethod())
    return;

  text_range_ = text_range;
  text_in_range_ = text_in_range;
  selection_range_ = selection_range;

  if (!UpdateCursorRect(rect, is_screen_coordinates))
    return;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnCaretBoundsChanged(this);
}

bool ArcImeService::ShouldEnableKeyEventForwarding() {
  return base::FeatureList::IsEnabled(
      chromeos::features::kArcPreImeKeyEventSupport);
}

void ArcImeService::SendKeyEvent(std::unique_ptr<ui::KeyEvent> key_event,
                                 KeyEventDoneCallback callback) {
  ui::InputMethod* const input_method = GetInputMethod();
  receiver_->SetCallback(std::move(callback));
  if (input_method)
    ignore_result(input_method->DispatchKeyEvent(key_event.get()));
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from ash::KeyboardControllerObserver
void ArcImeService::OnKeyboardAppearanceChanged(
    const ash::KeyboardStateDescriptor& state) {
  gfx::Rect new_bounds = state.occluded_bounds_in_screen;
  // Multiply by the scale factor. To convert from DIP to physical pixels.
  // The default scale factor is always used in Android side regardless of
  // dynamic scale factor in Chrome side because Chrome sends only the default
  // scale factor. You can find that in WaylandRemoteShell in
  // components/exo/wayland/server.cc. We can't send dynamic scale factor due to
  // difference between definition of DIP in Chrome OS and definition of DIP in
  // Android.
  gfx::Rect bounds_in_px =
      gfx::ScaleToEnclosingRect(new_bounds, GetDefaultDeviceScaleFactor());

  ime_bridge_->SendOnKeyboardAppearanceChanging(bounds_in_px, state.is_visible);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from ui::TextInputClient:

void ArcImeService::SetCompositionText(
    const ui::CompositionText& composition) {
  InvalidateSurroundingTextAndSelectionRange();
  has_composition_text_ = !composition.text.empty();
  ime_bridge_->SendSetCompositionText(composition);
}

uint32_t ArcImeService::ConfirmCompositionText(bool keep_selection) {
  if (!keep_selection) {
    InvalidateSurroundingTextAndSelectionRange();
  }
  has_composition_text_ = false;
  // Note: SendConfirmCompositonText() will commit the text and
  // keep the selection unchanged
  ime_bridge_->SendConfirmCompositionText();
  return UINT32_MAX;
}

void ArcImeService::ClearCompositionText() {
  InvalidateSurroundingTextAndSelectionRange();
  if (has_composition_text_) {
    has_composition_text_ = false;
    ime_bridge_->SendInsertText(base::string16());
  }
}

void ArcImeService::InsertText(const base::string16& text) {
  InvalidateSurroundingTextAndSelectionRange();
  has_composition_text_ = false;
  ime_bridge_->SendInsertText(text);
}

void ArcImeService::InsertChar(const ui::KeyEvent& event) {
  // When IME is blocked for the window, let Exo handle the event.
  if (arc_window_delegate_->IsImeBlocked(focused_arc_window_))
    return;

  // According to the document in text_input_client.h, InsertChar() is called
  // even when the text input type is NONE. We ignore such events, since for
  // ARC we are only interested in the event as a method of text input.
  if (ime_type_ == ui::TEXT_INPUT_TYPE_NONE ||
      ime_type_ == ui::TEXT_INPUT_TYPE_NULL) {
    return;
  }

  InvalidateSurroundingTextAndSelectionRange();

  // For apps that doesn't handle hardware keyboard events well, keys that are
  // typically on software keyboard and lack of them are fatal, namely,
  // unmodified enter and backspace keys are sent through IME.
  if (!HasModifier(&event) && !ShouldEnableKeyEventForwarding()) {
    if (event.key_code() ==  ui::VKEY_RETURN) {
      has_composition_text_ = false;
      ime_bridge_->SendInsertText(base::ASCIIToUTF16("\n"));
      return;
    }
    if (event.key_code() ==  ui::VKEY_BACK) {
      has_composition_text_ = false;
      ime_bridge_->SendInsertText(base::ASCIIToUTF16("\b"));
      return;
    }
  }

  if (!IsControlChar(&event) && !ui::IsSystemKeyModifier(event.flags())) {
    has_composition_text_ = false;
    ime_bridge_->SendInsertText(base::string16(1, event.GetText()));
  }
}

ui::TextInputType ArcImeService::GetTextInputType() const {
  if (arc_window_delegate_->IsImeBlocked(focused_arc_window_))
    return ui::TEXT_INPUT_TYPE_NONE;
  return ime_type_;
}

gfx::Rect ArcImeService::GetCaretBounds() const {
  return cursor_rect_;
}

bool ArcImeService::GetTextRange(gfx::Range* range) const {
  if (!text_range_.IsValid())
    return false;
  *range = text_range_;
  return true;
}

bool ArcImeService::GetEditableSelectionRange(gfx::Range* range) const {
  if (!selection_range_.IsValid())
    return false;
  *range = selection_range_;
  return true;
}

bool ArcImeService::GetTextFromRange(const gfx::Range& range,
                                     base::string16* text) const {
  // It's supposed that this method is called only from
  // InputMethod::OnCaretBoundsChanged(). In that method, the range obtained
  // from GetTextRange() is used as the argument of this method. To prevent an
  // unexpected usage, the check, |range != text_range_|, is added.
  if (!text_range_.IsValid() || range != text_range_)
    return false;
  *text = text_in_range_;
  return true;
}

void ArcImeService::EnsureCaretNotInRect(const gfx::Rect& rect_in_screen) {
  if (focused_arc_window_ == nullptr)
    return;
  aura::Window* top_level_window = focused_arc_window_->GetToplevelWindow();
  // If the window is not a notification, the window move is handled by
  // Android.
  if (top_level_window->type() != aura::client::WINDOW_TYPE_POPUP)
    return;
  wm::EnsureWindowNotInRect(top_level_window, rect_in_screen);
}

ui::TextInputMode ArcImeService::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection ArcImeService::GetTextDirection() const {
  return base::i18n::UNKNOWN_DIRECTION;
}

void ArcImeService::ExtendSelectionAndDelete(size_t before, size_t after) {
  InvalidateSurroundingTextAndSelectionRange();
  ime_bridge_->SendExtendSelectionAndDelete(before, after);
}

int ArcImeService::GetTextInputFlags() const {
  return ime_flags_;
}

bool ArcImeService::CanComposeInline() const {
  return true;
}

bool ArcImeService::GetCompositionCharacterBounds(
    uint32_t index, gfx::Rect* rect) const {
  return false;
}

bool ArcImeService::HasCompositionText() const {
  return has_composition_text_;
}

ui::TextInputClient::FocusReason ArcImeService::GetFocusReason() const {
  // TODO(https://crbug.com/824604): Determine how the current input client got
  // focused.
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::TextInputClient::FOCUS_REASON_OTHER;
}

bool ArcImeService::GetCompositionTextRange(gfx::Range* range) const {
  return false;
}

bool ArcImeService::SetEditableSelectionRange(const gfx::Range& range) {
  selection_range_ = range;
  ime_bridge_->SendSelectionRange(selection_range_);
  return true;
}

bool ArcImeService::DeleteRange(const gfx::Range& range) {
  return false;
}

bool ArcImeService::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

bool ArcImeService::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  return false;
}

ukm::SourceId ArcImeService::GetClientSourceForMetrics() const {
  // TODO(yhanada): Implement this method. crbug.com/752657
  NOTIMPLEMENTED_LOG_ONCE();
  return ukm::SourceId();
}

bool ArcImeService::ShouldDoLearning() {
  return is_personalized_learning_allowed_;
}

bool ArcImeService::SetCompositionFromExistingText(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  if (!range.IsBoundedBy(text_range_))
    return false;

  InvalidateSurroundingTextAndSelectionRange();
  has_composition_text_ = !range.is_empty();

  // The sent |range| might be already invalid if the textfield state in Android
  // side is changed simultaneously. It's okay because InputConnection's
  // setComposingRegion handles invalid region correctly.
  ime_bridge_->SendSetComposingRegion(range);
  return true;
}

gfx::Range ArcImeService::GetAutocorrectRange() const {
  // TODO(https:://crbug.com/1091088): Implement this method.
  return gfx::Range();
}

gfx::Rect ArcImeService::GetAutocorrectCharacterBounds() const {
  // TODO(https://crbug.com/952757): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

bool ArcImeService::SetAutocorrectRange(const base::string16& autocorrect_text,
                                        const gfx::Range& range) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Count",
                                TextInputClient::SubClass::kArcImeService);
  // TODO(https:://crbug.com/1091088): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void ArcImeService::OnDispatchingKeyEventPostIME(ui::KeyEvent* event) {
  if (ShouldEnableKeyEventForwarding() && receiver_->HasCallback()) {
    receiver_->DispatchKeyEventPostIME(event);
    event->SetHandled();
  }
}

void ArcImeService::ClearAutocorrectRange() {
  // TODO(https:://crbug.com/1091088): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ArcImeService::SetOverrideDefaultDeviceScaleFactorForTesting(
    base::Optional<double> scale_factor) {
  g_override_default_device_scale_factor = scale_factor;
}

void ArcImeService::InvalidateSurroundingTextAndSelectionRange() {
  text_range_ = gfx::Range::InvalidRange();
  text_in_range_ = base::string16();
  selection_range_ = gfx::Range::InvalidRange();
}

bool ArcImeService::UpdateCursorRect(const gfx::Rect& rect,
                                     bool is_screen_coordinates) {
  // Divide by the scale factor. To convert from physical pixels to DIP.
  // The default scale factor is always used because Android side is always
  // using the default scale factor regardless of dynamic scale factor in Chrome
  // side.
  gfx::Rect converted(
      gfx::ScaleToEnclosingRect(rect, 1 / GetDefaultDeviceScaleFactor()));

  // If the supplied coordinates are relative to the window, add the offset of
  // the window showing the ARC app.
  if (!is_screen_coordinates) {
    if (!focused_arc_window_)
      return false;
    converted.Offset(focused_arc_window_->GetToplevelWindow()
                         ->GetBoundsInScreen()
                         .OffsetFromOrigin());
  } else if (focused_arc_window_) {
    auto* window = focused_arc_window_->GetToplevelWindow();
    auto* widget = views::Widget::GetWidgetForNativeWindow(window);
    // Check fullscreen window as well because it's possible for ARC to request
    // frame regardless of window state.
    bool covers_display =
        widget && (widget->IsMaximized() || widget->IsFullscreen());
    if (covers_display) {
      auto* frame_view = widget->non_client_view()->frame_view();
      // The frame height will be subtracted from client bounds.
      gfx::Rect bounds =
          frame_view->GetWindowBoundsForClientBounds(gfx::Rect());
      converted.Offset(0, -bounds.y());
    }
  }

  if (cursor_rect_ == converted)
    return false;
  cursor_rect_ = converted;
  return true;
}

bool ArcImeService::ShouldSendUpdateToInputMethod() const {
  // New text input state received from Android should not be sent to
  // InputMethod when the focus is on a non-ARC window. Text input state updates
  // can be sent from Android anytime because there is a dummy input view in
  // Android which is synchronized with the text input on a non-ARC window.
  return focused_arc_window_ != nullptr;
}

}  // namespace arc
