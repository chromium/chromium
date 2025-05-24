// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/on_device_translation_internals/on_device_translation_internals_page_handler_impl.h"

#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/component_manager.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"

using on_device_translation_internals::mojom::LanguagePackInfo;
using on_device_translation_internals::mojom::LanguagePackInfoPtr;
using on_device_translation_internals::mojom::LanguagePackStatus;

using on_device_translation::ComponentManager;
using on_device_translation::kLanguagePackComponentConfigMap;
using on_device_translation::LanguagePackKey;
using on_device_translation::ToLanguageCode;

OnDeviceTranslationInternalsPageHandlerImpl::
    OnDeviceTranslationInternalsPageHandlerImpl(
        mojo::PendingReceiver<
            on_device_translation_internals::mojom::PageHandler> receiver,
        mojo::PendingRemote<on_device_translation_internals::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {
  // Initialize the pref change registrar.
  pref_change_registrar_.Init(g_browser_process->local_state());
  // Start listening to pref changes for language pack keys.
  for (const auto& it : kLanguagePackComponentConfigMap) {
    pref_change_registrar_.Add(
        GetRegisteredFlagPrefName(*it.second),
        base::BindRepeating(
            &OnDeviceTranslationInternalsPageHandlerImpl::OnPrefChanged,
            base::Unretained(this)));
    pref_change_registrar_.Add(
        GetComponentPathPrefName(*it.second),
        base::BindRepeating(
            &OnDeviceTranslationInternalsPageHandlerImpl::OnPrefChanged,
            base::Unretained(this)));
  }

  SendLanguagePackInfo();
}

OnDeviceTranslationInternalsPageHandlerImpl::
    ~OnDeviceTranslationInternalsPageHandlerImpl() = default;

void OnDeviceTranslationInternalsPageHandlerImpl::InstallLanguagePackage(
    uint32_t package_index) {
  if (package_index > static_cast<uint32_t>(LanguagePackKey::kMaxValue)) {
    return;
  }
  ComponentManager::GetInstance().RegisterTranslateKitLanguagePackComponent(
      static_cast<LanguagePackKey>(package_index));
}

void OnDeviceTranslationInternalsPageHandlerImpl::UninstallLanguagePackage(
    uint32_t package_index) {
  if (package_index > static_cast<uint32_t>(LanguagePackKey::kMaxValue)) {
    return;
  }
  ComponentManager::GetInstance().UninstallTranslateKitLanguagePackComponent(
      static_cast<LanguagePackKey>(package_index));
}

void OnDeviceTranslationInternalsPageHandlerImpl::SendLanguagePackInfo() {
  std::vector<LanguagePackInfoPtr> info_list;
  const auto registered_packs = ComponentManager::GetRegisteredLanguagePacks();
  const auto installed_packs = ComponentManager::GetInstalledLanguagePacks();

  for (const auto& it : kLanguagePackComponentConfigMap) {
    auto key = it.first;
    info_list.push_back(LanguagePackInfo::New(
        base::StrCat({std::string(ToLanguageCode(it.second->language1)), " - ",
                      std::string(ToLanguageCode(it.second->language2))}),
        registered_packs.contains(key)
            ? (installed_packs.contains(key) ? LanguagePackStatus::kInstalled
                                             : LanguagePackStatus::kInstalling)
            : LanguagePackStatus::kNotInstalled));
  }
  page_->OnLanguagePackStatus(std::move(info_list));
}

void OnDeviceTranslationInternalsPageHandlerImpl::OnPrefChanged(
    const std::string& pref_name) {
  SendLanguagePackInfo();
}
