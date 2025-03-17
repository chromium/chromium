// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_CONTROLLER_BASE_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_CONTROLLER_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

class PrefChangeRegistrar;
class PrefService;

namespace captions {

class CaptionBubbleController;
class CaptionBubbleSettings;

class CaptionControllerBase : public ui::NativeThemeObserver {
 public:
  class Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual std::unique_ptr<CaptionBubbleController>
    CreateCaptionBubbleController(
        CaptionBubbleSettings* caption_bubble_settings,
        const std::string& application_locale) = 0;

    virtual void AddCaptionStyleObserver(ui::NativeThemeObserver* observer) = 0;

    virtual void RemoveCaptionStyleObserver(
        ui::NativeThemeObserver* observer) = 0;

   protected:
    Delegate() = default;
  };
  CaptionControllerBase(const CaptionControllerBase&) = delete;
  CaptionControllerBase& operator=(const CaptionControllerBase&) = delete;

  ~CaptionControllerBase() override;

 protected:
  CaptionControllerBase(PrefService* profile_prefs,
                        const std::string& application_locale,
                        std::unique_ptr<Delegate> delegate = nullptr);

  void CreateUI();
  void DestroyUI();

  PrefService* profile_prefs() const;
  const std::string& application_locale() const;
  PrefChangeRegistrar* pref_change_registrar() const;
  CaptionBubbleController* caption_bubble_controller() const;

 private:
  virtual CaptionBubbleSettings* caption_bubble_settings() = 0;

  // ui::NativeThemeObserver:
  void OnCaptionStyleUpdated() override;

  const raw_ptr<PrefService> profile_prefs_;
  const std::string application_locale_;
  const std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<CaptionBubbleController> caption_bubble_controller_;
  const std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Whether the UI has been created. The UI is created asynchronously from the
  // feature being enabled--some implementations may wait for SODA to download
  // first. This flag ensures that the UI is not constructed or deconstructed
  // twice.
  bool is_ui_constructed_ = false;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_CONTROLLER_BASE_H_
