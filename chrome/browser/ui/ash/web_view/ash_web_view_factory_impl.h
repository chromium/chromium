// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WEB_VIEW_ASH_WEB_VIEW_FACTORY_IMPL_H_
#define CHROME_BROWSER_UI_ASH_WEB_VIEW_ASH_WEB_VIEW_FACTORY_IMPL_H_

#include "ash/public/cpp/ash_web_view_factory.h"

// Implements the singleton AshWebViewFactory used by Ash to work around
// dependency restrictions.
class AshWebViewFactoryImpl : public ash::AshWebViewFactory {
 public:
  AshWebViewFactoryImpl();
  ~AshWebViewFactoryImpl() override;

  AshWebViewFactoryImpl(const AshWebViewFactoryImpl&) = delete;
  AshWebViewFactoryImpl& operator=(const AshWebViewFactoryImpl&) = delete;

  // ash::AshWebViewFactory:
  std::unique_ptr<ash::AshWebView> Create(
      const ash::AshWebView::InitParams& params) override;
};

#endif  // CHROME_BROWSER_UI_ASH_WEB_VIEW_ASH_WEB_VIEW_FACTORY_IMPL_H_
