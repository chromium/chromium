// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_paths.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"

namespace component_updater {

namespace {

// This key gives the root directory of all the component installations.
static int g_components_preinstalled_root_key = -1;
static int g_components_preinstalled_root_key_alt = -1;
static int g_components_user_root_key = -1;

}  // namespace

bool PathProvider(int key, base::FilePath* result) {
  DCHECK_GT(g_components_user_root_key, 0);
  DCHECK_GT(g_components_preinstalled_root_key, 0);

  // Early exit here to prevent a potential infinite loop when we retrieve
  // the path for g_components_*_root_key.
  if (key < PATH_START || key > PATH_END)
    return false;

  switch (key) {
    case DIR_COMPONENT_PREINSTALLED:
      return base::PathService::Get(g_components_preinstalled_root_key, result);
    case DIR_COMPONENT_PREINSTALLED_ALT:
      return base::PathService::Get(g_components_preinstalled_root_key_alt,
                                    result);
    case DIR_COMPONENT_USER:
      return base::PathService::Get(g_components_user_root_key, result);
  }

  base::FilePath cur;
  if (!base::PathService::Get(g_components_user_root_key, &cur))
    return false;

  switch (key) {
    case DIR_COMPONENT_CLD2:
      cur = cur.Append(FILE_PATH_LITERAL("CLD"));
      break;
    case DIR_RECOVERY_BASE:
      cur = cur.Append(FILE_PATH_LITERAL("recovery"));
      break;
    case DIR_SWIFT_SHADER:
      cur = cur.Append(FILE_PATH_LITERAL("SwiftShader"));
      break;
    default:
      return false;
  }

  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider(int components_preinstalled_root_key,
                          int components_preinstalled_root_key_alt,
                          int components_user_root_key) {
  DCHECK_EQ(g_components_preinstalled_root_key, -1);
  DCHECK_EQ(g_components_preinstalled_root_key_alt, -1);
  DCHECK_EQ(g_components_user_root_key, -1);
  DCHECK_GT(components_preinstalled_root_key, 0);
  DCHECK_GT(components_preinstalled_root_key_alt, 0);
  DCHECK_GT(components_user_root_key, 0);
  DCHECK(components_preinstalled_root_key < PATH_START ||
         components_preinstalled_root_key > PATH_END);
  DCHECK(components_preinstalled_root_key_alt < PATH_START ||
         components_preinstalled_root_key_alt > PATH_END);
  DCHECK(components_user_root_key < PATH_START ||
         components_user_root_key > PATH_END);

  g_components_preinstalled_root_key = components_preinstalled_root_key;
  g_components_preinstalled_root_key_alt = components_preinstalled_root_key_alt;
  g_components_user_root_key = components_user_root_key;
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace component_updater
