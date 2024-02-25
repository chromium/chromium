// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/** Provides a means for validating whether some condition/criteria has been met. */
public interface Criteria {

    /** @return Whether the criteria this is testing has been satisfied. */
    public boolean isSatisfied();
}
