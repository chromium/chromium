// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extensions_toolbar_android.h"

#include <cstdint>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/extensions/extension_action_delegate_android.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions_manager.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/color/color_provider.h"
#include "ui/events/android/key_event_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/paint_vector_icon.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionAction_jni.h"
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsMenuButtonState_jni.h"
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsToolbarBridge_jni.h"
#include "chrome/browser/ui/android/extensions/jni_headers/RequestAccessButtonParams_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::WebContents;

namespace extensions {

ExtensionsToolbarAndroid::ExtensionsToolbarAndroid(
    BrowserWindowInterface* browser,
    const base::android::JavaRef<jobject>& java_object)
    : browser_(browser),
      toolbar_view_model_(std::make_unique<ExtensionsToolbarViewModel>(
          this,
          browser,
          ToolbarActionsModel::Get(browser_->GetProfile()))),
      scoped_toolbar_view_model_user_data_(browser->GetUnownedUserDataHost(),
                                           *toolbar_view_model_),
      keybinding_registry_(std::make_unique<ExtensionKeybindingRegistryAndroid>(
          browser_->GetProfile(),
          toolbar_view_model_.get())),
      java_object_(java_object) {
  toolbar_view_model_observation_.Observe(toolbar_view_model_.get());

  if (toolbar_view_model_->AreActionsInitialized()) {
    // We missed the observer call for initialization.
    OnActionsInitialized();
  }
}

ExtensionsToolbarAndroid::~ExtensionsToolbarAndroid() = default;

bool ExtensionsToolbarAndroid::HasActivePopup() {
  return Java_ExtensionsToolbarBridge_hasActivePopup(AttachCurrentThread(),
                                                     java_object_);
}

void ExtensionsToolbarAndroid::TriggerPopup(
    const ToolbarActionsModel::ActionId& action_id,
    std::unique_ptr<ExtensionViewHost> host) {
  Java_ExtensionsToolbarBridge_triggerPopup(
      AttachCurrentThread(), java_object_, action_id,
      reinterpret_cast<int64_t>(host.release()));
}

void ExtensionsToolbarAndroid::ShowContextMenu(
    const ToolbarActionsModel::ActionId& action_id) {
  Java_ExtensionsToolbarBridge_showContextMenu(AttachCurrentThread(),
                                               java_object_, action_id);
}

std::unique_ptr<ExtensionActionViewModel>
ExtensionsToolbarAndroid::CreateActionViewModel(
    const ToolbarActionsModel::ActionId& action_id,
    ExtensionsContainer* extensions_container) {
  return ExtensionActionViewModel::Create(
      action_id, browser_,
      std::make_unique<ExtensionActionDelegateAndroid>(browser_.get(),
                                                       action_id, this));
}

base::android::ScopedJavaLocalRef<jobject>
ExtensionsToolbarAndroid::GetRequestAccessButtonParams(
    JNIEnv* env,
    content::WebContents* web_contents) {
  ExtensionsToolbarViewModel::RequestAccessButtonParams params =
      toolbar_view_model_->GetRequestAccessButtonParams(web_contents);
  return Java_RequestAccessButtonParams_Constructor(env, params.extension_ids,
                                                    params.tooltip_text);
}

void ExtensionsToolbarAndroid::OnRequestAccessButtonParamsChanged(
    content::WebContents* web_contents) {
  Java_ExtensionsToolbarBridge_onRequestAccessButtonParamsChanged(
      AttachCurrentThread(), java_object_);
}

void ExtensionsToolbarAndroid::HideActivePopup() {
  return Java_ExtensionsToolbarBridge_hideActivePopup(AttachCurrentThread(),
                                                      java_object_);
}

void ExtensionsToolbarAndroid::CloseExtensionsMenuIfOpen() {
  Java_ExtensionsToolbarBridge_closeExtensionsMenuIfOpen(AttachCurrentThread(),
                                                         java_object_);
}

bool ExtensionsToolbarAndroid::CanShowToolbarActionPopupForAPICall(
    const std::string& action_id) {
  return Java_ExtensionsToolbarBridge_hasPoppedOutAction(AttachCurrentThread(),
                                                         java_object_);
}

void ExtensionsToolbarAndroid::ToggleExtensionsMenu() {
  // On Android, the menu is tied to the extensions menu button within Java and
  // therefore this method is not used.
  NOTIMPLEMENTED();
}

void ExtensionsToolbarAndroid::ShowManageExtensionsIPH() {
  Java_ExtensionsToolbarBridge_showManageExtensionsIPH(AttachCurrentThread(),
                                                       java_object_);
}

void ExtensionsToolbarAndroid::OnActionsInitialized() {
  for (const auto& action_id : toolbar_view_model_->GetAllActionIds()) {
    RegisterIconObserverForAction(action_id);
  }
  Java_ExtensionsToolbarBridge_onActionsInitialized(AttachCurrentThread(),
                                                    java_object_);
}

void ExtensionsToolbarAndroid::OnActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  RegisterIconObserverForAction(action_id);
  Java_ExtensionsToolbarBridge_onActionAdded(AttachCurrentThread(),
                                             java_object_, action_id);
}

void ExtensionsToolbarAndroid::OnActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  icon_subscriptions_.erase(action_id);
  Java_ExtensionsToolbarBridge_onActionRemoved(AttachCurrentThread(),
                                               java_object_, action_id);
}

void ExtensionsToolbarAndroid::OnActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  Java_ExtensionsToolbarBridge_onActionUpdated(AttachCurrentThread(),
                                               java_object_, action_id);
}

void ExtensionsToolbarAndroid::OnPinnedActionsChanged() {
  Java_ExtensionsToolbarBridge_onPinnedActionsChanged(AttachCurrentThread(),
                                                      java_object_);
}

void ExtensionsToolbarAndroid::OnActiveWebContentsChanged(
    bool /*is_same_document*/,
    content::WebContents* web_contents) {
  Java_ExtensionsToolbarBridge_onActiveWebContentsChanged(
      AttachCurrentThread(), java_object_, web_contents->GetJavaWebContents());
}

void ExtensionsToolbarAndroid::OnToolbarControlStateUpdated() {
  Java_ExtensionsToolbarBridge_onToolbarControlStateUpdated(
      AttachCurrentThread(), java_object_);
}

void ExtensionsToolbarAndroid::Destroy(JNIEnv* env) {
  delete this;
}

base::android::ScopedJavaLocalRef<jobject> ExtensionsToolbarAndroid::GetAction(
    JNIEnv* env,
    const ToolbarActionsModel::ActionId& action_id,
    content::WebContents* web_contents) {
  ToolbarActionViewModel* action =
      toolbar_view_model_->GetActionModelForId(action_id);

  auto hover_card_state = action->GetHoverCardState(web_contents);
  ExtensionActionViewModel::HoverCardUiState ui_state =
      action->GetHoverCardUiState(hover_card_state, web_contents);

  std::optional<std::string> site_access_title;
  if (ui_state.site_access_title.has_value()) {
    site_access_title = base::UTF16ToUTF8(ui_state.site_access_title.value());
  }

  std::optional<std::string> site_access_description;
  if (ui_state.site_access_description.has_value()) {
    site_access_description =
        base::UTF16ToUTF8(ui_state.site_access_description.value());
  }

  std::optional<std::string> policy_text;
  if (ui_state.policy_text.has_value()) {
    policy_text = base::UTF16ToUTF8(ui_state.policy_text.value());
  }

  base::android::ScopedJavaLocalRef<jobject> java_hover_card_state =
      Java_HoverCardState_Constructor(
          env, static_cast<int>(hover_card_state.site_access),
          site_access_title, site_access_description,
          static_cast<int>(hover_card_state.policy), policy_text);

  return Java_ExtensionAction_Constructor(
      env, action_id, base::UTF16ToUTF8(action->GetActionName()),
      base::UTF16ToUTF8(action->GetActionTitle(web_contents)),
      base::UTF16ToUTF8(action->GetAccessibleName(web_contents)),
      java_hover_card_state);
}

base::android::ScopedJavaLocalRef<jobject> ExtensionsToolbarAndroid::GetIcon(
    JNIEnv* env,
    const ToolbarActionsModel::ActionId& action_id,
    content::WebContents* web_contents,
    int canvas_width_dp,
    int canvas_height_dp,
    float scale_factor) {
  gfx::Size size(canvas_width_dp, canvas_height_dp);

  ToolbarActionViewModel* action =
      toolbar_view_model_->GetActionModelForId(action_id);
  DCHECK(action);
  ui::ImageModel model = action->GetIcon(web_contents, size);

  if (model.IsEmpty() || !model.IsImage()) {
    return nullptr;
  }

  gfx::ImageSkia image_skia = model.GetImage().AsImageSkia();

  const gfx::ImageSkiaRep& rep = image_skia.GetRepresentation(scale_factor);
  const SkBitmap& bitmap = rep.GetBitmap();

  if (bitmap.isNull()) {
    return nullptr;
  }

  return gfx::ConvertToJavaBitmap(bitmap);
}

std::vector<ToolbarActionsModel::ActionId>
ExtensionsToolbarAndroid::GetAllActionIds(JNIEnv* env) {
  const auto& ids = toolbar_view_model_->GetAllActionIds();
  return std::vector(ids.begin(), ids.end());
}

std::vector<ToolbarActionsModel::ActionId>
ExtensionsToolbarAndroid::GetPinnedActionIds(JNIEnv* env) {
  const auto& ids = toolbar_view_model_->GetPinnedActionIds();
  return std::vector(ids.begin(), ids.end());
}

namespace {

class MenuButtonIconSource : public gfx::CanvasImageSource {
 public:
  MenuButtonIconSource(const gfx::VectorIcon& icon,
                       int width,
                       int height,
                       SkColor color)
      : gfx::CanvasImageSource(gfx::Size(width, height)),
        icon_(icon),
        color_(color) {}
  MenuButtonIconSource(const MenuButtonIconSource&) = delete;
  MenuButtonIconSource& operator=(const MenuButtonIconSource&) = delete;
  ~MenuButtonIconSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    int icon_size = std::min(size().width(), size().height());
    int x = (size().width() - icon_size) / 2;
    int y = (size().height() - icon_size) / 2;
    canvas->Translate(gfx::Vector2d(x, y));
    gfx::PaintVectorIcon(canvas, *icon_, icon_size, color_);
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
  SkColor color_;
};

}  // namespace

base::android::ScopedJavaLocalRef<jobject>
ExtensionsToolbarAndroid::GetMenuButtonState(JNIEnv* env,
                                             content::WebContents* web_contents,
                                             int canvas_width_dp,
                                             int canvas_height_dp,
                                             float scale_factor,
                                             int color) {
  auto state = toolbar_view_model_->GetButtonState(*web_contents);

  std::u16string tooltip =
      ExtensionsToolbarViewModel::GetToolbarButtonTooltipText(state);
  std::u16string accessible_text =
      ExtensionsToolbarViewModel::GetToolbarButtonAccessibleText(state);

  const gfx::VectorIcon& icon =
      ExtensionsToolbarViewModel::GetToolbarButtonIcon(state);

  gfx::ImageSkia image_skia = gfx::ImageSkia(
      std::make_unique<MenuButtonIconSource>(
          icon, canvas_width_dp, canvas_height_dp, static_cast<SkColor>(color)),
      gfx::Size(canvas_width_dp, canvas_height_dp));
  const SkBitmap& bitmap =
      image_skia.GetRepresentation(scale_factor).GetBitmap();

  base::android::ScopedJavaLocalRef<jobject> java_bitmap;
  if (!bitmap.isNull()) {
    java_bitmap = gfx::ConvertToJavaBitmap(bitmap);
  }

  return Java_ExtensionsMenuButtonState_Constructor(
      env, base::UTF16ToUTF8(tooltip), base::UTF16ToUTF8(accessible_text),
      java_bitmap);
}

bool ExtensionsToolbarAndroid::HandleKeyDownEvent(
    JNIEnv* env,
    const ui::KeyEventAndroid& key_event) {
  return keybinding_registry_->HandleKeyDownEvent(key_event);
}

bool ExtensionsToolbarAndroid::IsActionDraggable(
    JNIEnv* env,
    const ToolbarActionsModel::ActionId& action_id) {
  return toolbar_view_model_->IsActionDraggable(action_id);
}

void ExtensionsToolbarAndroid::OnRequestAccessButtonClicked(
    JNIEnv* env,
    content::WebContents* web_contents) {
  ExtensionsToolbarViewModel::RequestAccessButtonParams params =
      toolbar_view_model_->GetRequestAccessButtonParams(web_contents);

  toolbar_view_model_->GrantSiteAccess(web_contents, params.extension_ids);
}

void ExtensionsToolbarAndroid::ExecuteUserAction(
    const ToolbarActionsModel::ActionId& action_id,
    ToolbarActionViewModel::InvocationSource source) {
  toolbar_view_model_->ExecuteUserAction(action_id, source);
}

void ExtensionsToolbarAndroid::RegisterIconObserverForAction(
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewModel* action_model =
      toolbar_view_model_->GetActionModelForId(action_id);
  DCHECK(action_model);

  icon_subscriptions_.insert_or_assign(
      action_id, action_model->RegisterIconUpdateObserver(base::BindRepeating(
                     &ExtensionsToolbarAndroid::OnActionIconUpdated,
                     base::Unretained(this), action_id)));
}

void ExtensionsToolbarAndroid::OnActionIconUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  Java_ExtensionsToolbarBridge_onActionUpdated(AttachCurrentThread(),
                                               java_object_, action_id);
}

void ExtensionsToolbarAndroid::MovePinnedAction(
    const ToolbarActionsModel::ActionId& action_id,
    int target_index) {
  toolbar_view_model_->MovePinnedAction(action_id, target_index);
}

static int64_t JNI_ExtensionsToolbarBridge_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_object,
    int64_t j_browser_window_interface) {
  BrowserWindowInterface* browser =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_interface);
  return reinterpret_cast<int64_t>(
      new ExtensionsToolbarAndroid(browser, java_object));
}

}  // namespace extensions

DEFINE_JNI(ExtensionAction)
DEFINE_JNI(ExtensionsToolbarBridge)
