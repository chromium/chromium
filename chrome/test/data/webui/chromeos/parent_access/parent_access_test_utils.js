// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionApprovalsParams, ExtensionPermission, ParentAccessParams, ParentAccessParams_FlowType, WebApprovalsParams} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';

function strToMojoString16(str) {
  return {data: str.split('').map(ch => ch.charCodeAt(0))};
}

/**
 * @returns {ParentAccessParams}
 */
export function buildWebApprovalsParams() {
  const parentAccessParams = new ParentAccessParams();
  parentAccessParams.flowType = ParentAccessParams_FlowType.kWebsiteAccess;
  const webApprovalsParams = new WebApprovalsParams();
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
  const parentAccessParams = new ParentAccessParams();
  parentAccessParams.flowType = ParentAccessParams_FlowType.kExtensionAccess;
  parentAccessParams.isDisabled = isDisabled;

  const extensionApprovalsParams = new ExtensionApprovalsParams();
  extensionApprovalsParams.extensionName = strToMojoString16('Extension name');
  extensionApprovalsParams.iconPngBytes = [];
  extensionApprovalsParams.childDisplayName = strToMojoString16('Child Name');

  const permission = new ExtensionPermission();
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
  const parentAccessParams = new ParentAccessParams();
  parentAccessParams.flowType = ParentAccessParams_FlowType.kExtensionAccess;
  parentAccessParams.isDisabled = isDisabled;

  const extensionApprovalsParams = new ExtensionApprovalsParams();
  extensionApprovalsParams.extensionName = strToMojoString16('Extension name');
  extensionApprovalsParams.iconPngBytes = [];
  extensionApprovalsParams.childDisplayName = strToMojoString16('Child Name');
  extensionApprovalsParams.permissions = [];

  parentAccessParams.flowTypeParams = {extensionApprovalsParams};
  return parentAccessParams;
}
