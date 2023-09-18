// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ParentAccessParams_FlowType} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';

function strToMojoString16(str) {
  return {data: str.split('').map(ch => ch.charCodeAt(0))};
}

// TODO(b:293655835): Use ParentAccessParams types when this file is migrated to
// TS.

/**
 * @returns {ParentAccessParams}
 */
export function buildWebApprovalsParams() {
  const parentAccessParams = {};
  parentAccessParams.flowType = ParentAccessParams_FlowType.kWebsiteAccess;
  const webApprovalsParams = {};
  webApprovalsParams.url = {url: 'https://testing.com'};
  webApprovalsParams.childDisplayName = strToMojoString16('Child Name');
  webApprovalsParams.faviconPngBytes = [];
  parentAccessParams.flowTypeParams = {webApprovalsParams};
  return parentAccessParams;
}

/**
 * @param {boolean=} isDisabled If the disabled UI should be shown.
 * @param {boolean=} hasDetails If the permission should have details.
 * @returns {ParentAccessParams}
 */
export function buildExtensionApprovalsParamsWithPermissions(
    isDisabled = false, hasDetails = false) {
  const parentAccessParams = {};
  parentAccessParams.flowType = ParentAccessParams_FlowType.kExtensionAccess;
  parentAccessParams.isDisabled = isDisabled;

  const extensionApprovalsParams = {};
  extensionApprovalsParams.extensionName = strToMojoString16('Extension name');
  extensionApprovalsParams.iconPngBytes = [];
  extensionApprovalsParams.childDisplayName = strToMojoString16('Child Name');

  const permission = {};
  permission.permission = strToMojoString16('permission');
  if (hasDetails) {
    permission.details = strToMojoString16('details');
  } else {
    permission.details = strToMojoString16('');
  }
  extensionApprovalsParams.permissions = [permission];

  parentAccessParams.flowTypeParams = {extensionApprovalsParams};
  return parentAccessParams;
}

/**
 * @param {boolean=} isDisabled If the disabled UI should be shown.
 * @returns {ParentAccessParams}
 */
export function buildExtensionApprovalsParamsWithoutPermissions(
    isDisabled = false) {
  const parentAccessParams = {};
  parentAccessParams.flowType = ParentAccessParams_FlowType.kExtensionAccess;
  parentAccessParams.isDisabled = isDisabled;

  const extensionApprovalsParams = {};
  extensionApprovalsParams.extensionName = strToMojoString16('Extension name');
  extensionApprovalsParams.iconPngBytes = [];
  extensionApprovalsParams.childDisplayName = strToMojoString16('Child Name');
  extensionApprovalsParams.permissions = [];

  parentAccessParams.flowTypeParams = {extensionApprovalsParams};
  return parentAccessParams;
}
