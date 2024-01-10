// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_CREATOR_H_
#define CHROME_BROWSER_VR_UI_SCENE_CREATOR_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "ui/gfx/geometry/size_f.h"

namespace vr {

class Ui;
class UiScene;
struct Model;

// The scene manager creates our scene hierarchy.
class UiSceneCreator {
 public:
  UiSceneCreator(UiScene* scene, Ui* ui, Model* model);

  UiSceneCreator(const UiSceneCreator&) = delete;
  UiSceneCreator& operator=(const UiSceneCreator&) = delete;

  ~UiSceneCreator();

  void CreateScene();

 private:
  void CreateWebVrRoot();
  void CreateViewportAwareRoot();
  void CreateWebVrSubtree();
  void CreateWebVrOverlayElements();
  void CreateWebVrTimeoutScreen();
  void CreateExternalPromptNotifcationOverlay();

  raw_ptr<UiScene> scene_;
  raw_ptr<Ui> ui_;
  raw_ptr<Model> model_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_CREATOR_H_
