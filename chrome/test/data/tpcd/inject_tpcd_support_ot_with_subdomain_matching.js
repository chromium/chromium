// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generate this token with the command:
// tools/origin_trials/generate_token.py https://example.test Tpcd --version 3 --is-third-party --is-subdomain --expire-timestamp=2000000000
const THIRD_PARTY_TOKEN = 'A5aWuMC4g54DnaEextm+MBs49xtqqoWOH2KeYitTkkMjU+uPDfjf3Vu7dcdmf5lwJIs4lKNZhgdNDF+d+omy4QoAAAB6eyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiVHBjZCIsICJleHBpcnkiOiAyMDAwMDAwMDAwLCAiaXNTdWJkb21haW4iOiB0cnVlLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZX0=';

const tokenElement = document.createElement('meta');
tokenElement.httpEquiv = 'origin-trial';
tokenElement.content = THIRD_PARTY_TOKEN;
document.head.appendChild(tokenElement);