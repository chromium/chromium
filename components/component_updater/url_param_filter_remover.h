// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_URL_PARAM_FILTER_REMOVER_H_
#define COMPONENTS_COMPONENT_UPDATER_URL_PARAM_FILTER_REMOVER_H_

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

// TODO(crbug.com/40269623): Remove in M114 or later.
void DeleteUrlParamFilter(const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_URL_PARAM_FILTER_REMOVER_H_
