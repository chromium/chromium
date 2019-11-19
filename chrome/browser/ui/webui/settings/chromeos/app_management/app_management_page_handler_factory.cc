// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_page_handler_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/resource/resource_bundle.h"

AppManagementPageHandlerFactory::AppManagementPageHandlerFactory(
    Profile* profile)
    : profile_(profile) {}

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

  page_handler_ = std::make_unique<AppManagementPageHandler>(
      std::move(receiver), std::move(page), profile_);
}
