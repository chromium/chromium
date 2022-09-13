// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;

import org.chromium.base.Function;

/**
 * Determines whether the activity was launched by GSA.
 */
public interface AssistantIsGsaFunction extends Function<Activity, Boolean> {}
