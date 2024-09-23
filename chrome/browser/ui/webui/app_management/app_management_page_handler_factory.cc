// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_chromeos.h"
#else
#include "chrome/browser/ui/webui/app_management/web_app_settings_page_handler.h"
#endif

AppManagementPageHandlerFactory::AppManagementPageHandlerFactory(
    Profile* profile,
    std::unique_ptr<AppManagementPageHandlerBase::Delegate> delegate)
    : profile_(profile), delegate_(std::move(delegate)) {}

AppManagementPageHandlerFactory::~AppManagementPageHandlerFactory() = default;

void AppManagementPageHandlerFactory::Bind(
    mojo::PendingReceiver<app_management::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();

  page_factory_receiver_.Bind(std::move(receiver));
}

void AppManagementPageHandlerFactory::CreatePageHandler(
    mojo::PendingRemote<app_management::mojom::Page> page,
    mojo::PendingReceiver<app_management::mojom::PageHandler> receiver) {
  DCHECK(page);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  page_handler_ = std::make_unique<AppManagementPageHandlerChromeOs>(
      std::move(receiver), std::move(page), profile_, *delegate_);
#else
  page_handler_ = std::make_unique<WebAppSettingsPageHandler>(
      std::move(receiver), std::move(page), profile_, *delegate_);
#endif
}
