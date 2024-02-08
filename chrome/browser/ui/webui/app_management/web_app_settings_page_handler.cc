// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/web_app_settings_page_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

WebAppSettingsPageHandler::WebAppSettingsPageHandler(
    mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_management::mojom::Page> page,
    Profile* profile,
    AppManagementPageHandlerBase::Delegate& delegate)
    : AppManagementPageHandlerBase(std::move(receiver),
                                   std::move(page),
                                   profile,
                                   delegate) {}

WebAppSettingsPageHandler::~WebAppSettingsPageHandler() = default;
