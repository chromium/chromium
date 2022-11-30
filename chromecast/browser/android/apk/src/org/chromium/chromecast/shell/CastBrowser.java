// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.components.module_installer.builder.ModuleInterface;

/**
 * Interface into the cast_browser module.
 *  Defined to satisfy the module interface requirement.
 *  */
@ModuleInterface(module = "cast_browser", impl = "org.chromium.chromecast.shell.CastBrowserImpl")
public interface CastBrowser {}
