// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_PATHS_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_PATHS_H_

namespace component_updater {

enum {
  PATH_START = 10000,
  DIR_COMPONENT_PREINSTALLED = PATH_START,  // Directory that contains component
                                            // implementations installed by the
                                            // Chrome installer or package
                                            // manager.
  DIR_COMPONENT_PREINSTALLED_ALT,           // A second preinstalled directory,
                                            // necessary because some components
                                            // live in a distinct directory on
                                            // OS X. On other platforms, this
                                            // ultimately is equivalent to
                                            // DIR_COMPONENT_PREINSTALLED.
  DIR_COMPONENT_USER,                       // Directory that contains user-wide
                                            // (component-updater-installer)
                                            // component implementations.
  // The following paths live in the user directory only, and point to the base
  // installation directory for the component.
  DIR_COMPONENT_CLD2,  // The Compact Language Detector.
  DIR_RECOVERY_BASE,   // The Recovery.
  DIR_SWIFT_SHADER,    // The SwiftShader.
  PATH_END
};

// Call once to register the provider for the path keys defined above.
// |components_system_root_key| is the path provider key defining where bundled
// components are already installed system-wide.
// |components_system_root_key_alt| is the path provider key defining an
// alternate location where bundled components are already installed
// system-wide. On most platforms this is the directory in which Chrome plug-ins
// are stored; on platforms where there is no good alternate value, callers
// should provide the same value that they use for |components_system_root_key|.
// |components_user_root_key| is the path provider key defining where the
// component updater should install new versions of components.
void RegisterPathProvider(int components_system_root_key,
                          int components_system_root_key_alt,
                          int components_user_root_key);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_PATHS_H_
