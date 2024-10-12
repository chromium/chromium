// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ON_DEVICE_TRANSLATION_INTERNALS_ON_DEVICE_TRANSLATION_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_ON_DEVICE_TRANSLATION_INTERNALS_ON_DEVICE_TRANSLATION_INTERNALS_PAGE_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/on_device_translation_internals/on_device_translation_internals.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Handler for the internals page to receive methods and sends the status
// update message.
class OnDeviceTranslationInternalsPageHandlerImpl
    : public on_device_translation_internals::mojom::PageHandler {
 public:
  OnDeviceTranslationInternalsPageHandlerImpl(
      mojo::PendingReceiver<on_device_translation_internals::mojom::PageHandler>
          receiver,
      mojo::PendingRemote<on_device_translation_internals::mojom::Page> page);
  ~OnDeviceTranslationInternalsPageHandlerImpl() override;

  OnDeviceTranslationInternalsPageHandlerImpl(
      const OnDeviceTranslationInternalsPageHandlerImpl&) = delete;
  OnDeviceTranslationInternalsPageHandlerImpl& operator=(
      const OnDeviceTranslationInternalsPageHandlerImpl&) = delete;

  // on_device_translation_internals::mojom::PageHandler:
  void InstallLanguagePackage(uint32_t package_index) override;
  void UninstallLanguagePackage(uint32_t package_index) override;

 private:
  void SendLanguagePackInfo();

  void OnPrefChanged(const std::string& pref_name);

  PrefChangeRegistrar pref_change_registrar_;

  mojo::Receiver<on_device_translation_internals::mojom::PageHandler> receiver_;
  mojo::Remote<on_device_translation_internals::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ON_DEVICE_TRANSLATION_INTERNALS_ON_DEVICE_TRANSLATION_INTERNALS_PAGE_HANDLER_IMPL_H_
