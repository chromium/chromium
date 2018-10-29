// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_HANDLER_FACTORY_MUS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_HANDLER_FACTORY_MUS_H_

#include "ash/public/cpp/immersive/immersive_handler_factory.h"
#include "base/macros.h"

class ImmersiveHandlerFactoryMus : public ash::ImmersiveHandlerFactory {
 public:
  ImmersiveHandlerFactoryMus();
  ~ImmersiveHandlerFactoryMus() override;

  // ImmersiveHandlerFactory:
  std::unique_ptr<ash::ImmersiveGestureHandler> CreateGestureHandler(
      ash::ImmersiveFullscreenController* controller) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveHandlerFactoryMus);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_HANDLER_FACTORY_MUS_H_
