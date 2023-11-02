// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.externalauth;

/**
 * Instantiable version of {@link ExternalAuthGoogleDelegate}, don't add anything to this class!
 * Downstream targets may provide a different implementation. In GN, we specify that
 * {@link ExternalAuthGoogleDelegate} is compiled separately from its implementation; other
 * projects may specify a different ExternalAuthGoogleDelegateImpl via GN.
 */
public class ExternalAuthGoogleDelegateImpl extends ExternalAuthGoogleDelegate {}
