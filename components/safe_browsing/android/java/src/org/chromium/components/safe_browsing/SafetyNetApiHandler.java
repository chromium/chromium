// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

/**
 * Java interface that a SafetyNetApiHandler must implement when used with
 * {@code SafeBrowsingApiBridge}.
 * TODO(crbug.com/1444515): Port SafeBrowsingApiHandler to this interface.
 */
public interface SafetyNetApiHandler extends SafeBrowsingApiHandler {}
