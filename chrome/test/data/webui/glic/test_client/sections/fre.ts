// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HostCapability} from '/glic/glic_api/glic_api.js';

import {client, getBrowser, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

async function onIsOnboardingCompleted(completed: boolean) {
  if (completed) {
    $.content.style = '';
    $.onboarding.style = 'display:none';
  }
}

client.getInitialized().then(async () => {
  const capabilities = await getBrowser()!.getHostCapabilities?.();
  if (!capabilities) {
    logMessage('host capabilities are not available');
    return;
  }

  if (capabilities.has(HostCapability.TRUST_FIRST_ONBOARDING_ARM2)) {
    $.content.style = 'display:none';
    $.onboarding.style = '';
    $.onboarding.addEventListener('click', async () => {
      await getBrowser()!.setOnboardingCompleted?.();
    });
  }

  await getBrowser()!.isOnboardingCompleted?.().subscribe(
      onIsOnboardingCompleted);
});
