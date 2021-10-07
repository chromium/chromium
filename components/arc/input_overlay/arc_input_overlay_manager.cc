// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/arc_input_overlay_manager.h"

#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace arc {
namespace {

// Singleton factory for ArcInputOverlayManager.
class ArcInputOverlayManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInputOverlayManager,
          ArcInputOverlayManagerFactory> {
 public:
  static constexpr const char* kName = "ArcInputOverlayManagerFactory";

  static ArcInputOverlayManagerFactory* GetInstance() {
    return base::Singleton<ArcInputOverlayManagerFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcInputOverlayManagerFactory>;
  ArcInputOverlayManagerFactory() = default;
  ~ArcInputOverlayManagerFactory() override = default;
};

}  // namespace

class ArcInputOverlayManager::InputMethodObserver
    : public ui::InputMethodObserver {
 public:
  explicit InputMethodObserver(ArcInputOverlayManager* owner) : owner_(owner) {}
  InputMethodObserver(const InputMethodObserver&) = delete;
  InputMethodObserver& operator=(const InputMethodObserver&) = delete;
  ~InputMethodObserver() override = default;

  // ui::InputMethodObserver overrides:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {
    owner_->is_text_input_active_ =
        client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE &&
        client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NULL;
  }
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {
    owner_->input_method_ = nullptr;
  }
  void OnShowVirtualKeyboardIfEnabled() override {}

 private:
  ArcInputOverlayManager* const owner_;
};

// static
ArcInputOverlayManager* ArcInputOverlayManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInputOverlayManagerFactory::GetForBrowserContext(context);
}

ArcInputOverlayManager::ArcInputOverlayManager(
    content::BrowserContext* browser_context,
    ArcBridgeService* arc_bridge_service)
    : input_method_observer_(std::make_unique<InputMethodObserver>(this)) {
  if (aura::Env::HasInstance())
    env_observation_.Observe(aura::Env::GetInstance());
}

ArcInputOverlayManager::~ArcInputOverlayManager() = default;

void ArcInputOverlayManager::ReadData(const std::string& package_name,
                                      aura::Window* top_level_window) {
  absl::optional<int> resource_id = GetInputOverlayResourceId(package_name);
  if (!resource_id)
    return;
  const base::StringPiece json_file =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          resource_id.value());
  if (json_file.empty()) {
    LOG(WARNING) << "No content for: " << package_name;
    return;
  }
  base::JSONReader::ValueWithError result =
      base::JSONReader::ReadAndReturnValueWithError(json_file);
  DCHECK(result.value) << "Could not load input overlay data file: "
                       << result.error_message;
  if (!result.value)
    return;

  base::Value& root = result.value.value();
  std::unique_ptr<TouchInjector> injector =
      std::make_unique<TouchInjector>(top_level_window);
  injector->ParseActions(root);
  input_overlay_enabled_windows_.emplace(top_level_window, std::move(injector));
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::EnvObserver:
void ArcInputOverlayManager::OnWindowInitialized(aura::Window* new_window) {
  if (window_observations_.IsObservingSource(new_window))
    return;

  window_observations_.AddObservation(new_window);
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::WindowObserver:
void ArcInputOverlayManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (!window || key != ash::kArcPackageNameKey)
    return;

  auto* top_level_window = window->GetToplevelWindow();
  if (top_level_window &&
      !input_overlay_enabled_windows_.contains(top_level_window)) {
    auto* package_name = window->GetProperty(ash::kArcPackageNameKey);
    if (!package_name || package_name->empty())
      return;
    ReadData(*package_name, top_level_window);

    // Add input method observer if it is not added.
    if (!input_method_ && window->GetHost()) {
      input_method_ = window->GetHost()->GetInputMethod();
      if (input_method_)
        input_method_->AddObserver(input_method_observer_.get());
    }
  }
}

void ArcInputOverlayManager::OnWindowDestroying(aura::Window* window) {
  if (input_overlay_enabled_windows_.contains(window))
    input_overlay_enabled_windows_.erase(window);

  if (window_observations_.IsObservingSource(window))
    window_observations_.RemoveObservation(window);
}

////////////////////////////////////////////////////////////////////////////////
// KeyedService:
void ArcInputOverlayManager::Shutdown() {
  if (input_method_) {
    input_method_->RemoveObserver(input_method_observer_.get());
    input_method_ = nullptr;
  }
}

}  // namespace arc
