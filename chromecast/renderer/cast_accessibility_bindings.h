// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_ACCESSIBILITY_BINDINGS_H_
#define CHROMECAST_RENDERER_CAST_ACCESSIBILITY_BINDINGS_H_

#include "base/memory/ptr_util.h"
#include "chromecast/common/mojom/accessibility.mojom.h"
#include "chromecast/renderer/native_bindings_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "v8/include/v8.h"

namespace chromecast {

class FeatureManager;

namespace shell {

// Renderer side implementation of JS available bindings for accessibility
// related functions.
class CastAccessibilityBindings : public CastBinding,
                                  public mojom::CastAccessibilityClient {
 public:
  CastAccessibilityBindings(content::RenderFrame* render_frame,
                            const FeatureManager* feature_manager);
  ~CastAccessibilityBindings() override;
  CastAccessibilityBindings(const CastAccessibilityBindings&) = delete;
  CastAccessibilityBindings& operator=(const CastAccessibilityBindings&) =
      delete;

 private:
  // content::RenderFrameObserver implementation:
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;

  // Typedefs for v8 objects that need to run between tasks.
  // v8::Global has move semantics, it does not imply unique ownership over an
  // v8 object. You can create multiple v8::Globals from the same v8::Local.
  using PersistedContext = v8::Global<v8::Context>;
  using PersistedResolver = v8::Global<v8::Promise::Resolver>;

  // CastBinding implementation:
  void Install(v8::Local<v8::Object> cast_platform,
               v8::Isolate* isolate) override;

  // Bind incoming accessibility requests with this implementation.
  void OnCastAccessibilityClientRequest(
      mojo::PendingReceiver<shell::mojom::CastAccessibilityClient> request);

  void OnConnectionError();
  bool BindAccessibility();

  void SetColorInversion(bool enable);
  void SetScreenReader(bool enable);
  void SetMagnificationGesture(bool enable);

  v8::Local<v8::Value> GetAccessibilitySettings();

  void SetScreenReaderSettingChangedHandler(v8::Local<v8::Function> handler);
  void SetColorInversionSettingChangedHandler(v8::Local<v8::Function> handler);
  void SetMagnificationGestureSettingChangedHandler(
      v8::Local<v8::Function> handler);

  void OnGetAccessibilitySettings(PersistedResolver resolver,
                                  PersistedContext original_context,
                                  mojom::AccessibilitySettingsPtr settings);

  void AccessibilitySettingChanged(
      v8::UniquePersistent<v8::Function>* handler_function,
      bool new_value);

  // mojom::CastAccessibilityClient implementation
  void ScreenReaderSettingChanged(bool new_value) override;
  void ColorInversionSettingChanged(bool new_value) override;
  void MagnificationGestureSettingChanged(bool new_value) override;

  mojo::Remote<mojom::CastAccessibilityService> accessibility_service_;

  const FeatureManager* feature_manager_;

  v8::UniquePersistent<v8::Function> screen_reader_setting_changed_handler_;
  v8::UniquePersistent<v8::Function> color_inversion_setting_changed_handler_;
  v8::UniquePersistent<v8::Function>
      magnification_gesture_setting_changed_handler_;

  service_manager::BinderRegistry registry_;

  mojo::ReceiverSet<shell::mojom::CastAccessibilityClient> bindings_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_ACCESSIBILITY_BINDINGS_H_
