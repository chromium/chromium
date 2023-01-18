// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_CREATOR_H_
#define CHROME_BROWSER_VR_UI_SCENE_CREATOR_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/text_input.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
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
                 TextInputDelegate* text_input_delegate,
                 AudioDelegate* audio_delegate,
                 Model* model);

  UiSceneCreator(const UiSceneCreator&) = delete;
  UiSceneCreator& operator=(const UiSceneCreator&) = delete;

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
  void Create2dBrowsingHostedUi();
  void CreateExternalPromptNotifcationOverlay();

  raw_ptr<UiBrowserInterface> browser_;
  raw_ptr<UiScene> scene_;
  raw_ptr<Ui> ui_;
  raw_ptr<ContentInputDelegate> content_input_delegate_;
  raw_ptr<TextInputDelegate> text_input_delegate_;
  raw_ptr<AudioDelegate> audio_delegate_;
  raw_ptr<Model> model_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_CREATOR_H_
