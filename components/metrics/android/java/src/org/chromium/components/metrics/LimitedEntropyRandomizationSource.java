// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.metrics;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;

import java.security.SecureRandom;

/**
 * Responsible for generating new limited entropy randomization source values.
 *
 * <p>More specifically, generates a random 128-bit value formatted as a 32-character uppercase
 * hexadecimal string, similar to {@code EntropyState::GenerateLimitedEntropyRandomizationSource()}
 * in C++. These strings are used to randomize clients across (A) limited-entropy-mode layer slots
 * and (B) the groups of limited-layer-constrained studies.
 */
@NullMarked
public class LimitedEntropyRandomizationSource {
    private LimitedEntropyRandomizationSource() {}

    // Generates and returns a random limited entropy randomization source value.
    // LINT.IfChange(generateValue)
    public static String generateValue() {
        SecureRandom random = new SecureRandom();
        long high;
        long low;
        do {
            high = random.nextLong();
            low = random.nextLong();
        }
        // The C++ implementation CHECKs if the token would be all 0s. See
        // https://crrev.com/c/4099384. Instead of CHECKing here, produce a new token.
        while (high == 0 && low == 0);
        return new Token(high, low).toString();
    }
    // LINT.ThenChange(/components/metrics/entropy_state.cc:generate_limited_entropy_randomization_source)
}
