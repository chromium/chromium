// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CONTROLLER_H_

namespace ash::babelorca {

class BabelOrcaController {
 public:
  BabelOrcaController(const BabelOrcaController&) = delete;
  BabelOrcaController& operator=(const BabelOrcaController&) = delete;

  virtual ~BabelOrcaController() = default;

  virtual void OnSessionStarted() = 0;

  virtual void OnSessionEnded() = 0;

  virtual void OnSessionCaptionConfigUpdated(bool session_captions_enabled,
                                             bool translations_enabled) = 0;

  virtual void OnLocalCaptionConfigUpdated(bool local_captions_enabled) = 0;
 protected:
  BabelOrcaController() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_CONTROLLER_H_
