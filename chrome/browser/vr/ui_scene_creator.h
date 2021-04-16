// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_CREATOR_H_
#define CHROME_BROWSER_VR_UI_SCENE_CREATOR_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/text_input.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "ui/gfx/geometry/size_f.h"

namespace vr {

class ContentInputDelegate;
class Ui;
class UiBrowserInterface;
class UiScene;
struct Model;

// The scene manager creates our scene hierarchy.
class UiSceneCreator {
 public:
  UiSceneCreator(UiBrowserInterface* browser,
                 UiScene* scene,
                 Ui* ui,
                 ContentInputDelegate* content_input_delegate,
                 KeyboardDelegate* keyboard_delegate,
                 TextInputDelegate* text_input_delegate,
                 AudioDelegate* audio_delegate,
                 Model* model);
  ~UiSceneCreator();

  void CreateScene();

 private:
  void Create2dBrowsingSubtreeRoots();
  void CreateWebVrRoot();
  void CreateSystemIndicators();
  void CreateContentQuad();
  void CreateBackground();
  void CreateViewportAwareRoot();
  void CreateUrlBar();
  void CreateOverflowMenu();
  void CreateOmnibox();
  void CreateCloseButton();
  void CreatePrompts();
  void CreateToasts();
  void CreateVoiceSearchUiGroup();
  void CreateContentRepositioningAffordance();
  void CreateWebVrSubtree();
  void CreateWebVrOverlayElements();
  void CreateWebVrTimeoutScreen();
  void CreateControllers();
  void CreateKeyboard();
  void Create2dBrowsingHostedUi();
  void CreateExternalPromptNotifcationOverlay();

  UiBrowserInterface* browser_;
  UiScene* scene_;
  Ui* ui_;
  ContentInputDelegate* content_input_delegate_;
  KeyboardDelegate* keyboard_delegate_;
  TextInputDelegate* text_input_delegate_;
  AudioDelegate* audio_delegate_;
  Model* model_;

  DISALLOW_COPY_AND_ASSIGN(UiSceneCreator);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_CREATOR_H_
