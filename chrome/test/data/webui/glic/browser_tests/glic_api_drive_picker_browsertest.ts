// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path: chrome/browser/glic/host/glic_api_drive_picker_browsertest.cc

import {DrivePickerErrorReason, FileUploadPolicyState, HostCapability} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertTrue, assertUndefined, testMain} from './browser_test_base.js';

class GlicApiDrivePickerBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testShowDrivePicker() {
    assertDefined(this.host.getHostCapabilities);
    assertTrue(
        this.host.getHostCapabilities().has(HostCapability.DRIVE_PICKER));
    assertDefined(this.host.showDrivePicker);
    const result = await this.host.showDrivePicker({
      accountId: this.testParams,
    });
    assertUndefined(result.errorReason);
    assertDefined(result.files);
    assertEquals(2, result.files.length);
    assertEquals('file-1', result.files[0]!.id);
    assertEquals('Q3 Report.pdf', result.files[0]!.title);
    assertEquals('application/pdf', result.files[0]!.mimeType);
    assertEquals(4096, result.files[0]!.sizeBytes);
    assertEquals('rk-1', result.files[0]!.resourceKey);
    assertEquals('doc-2', result.files[1]!.id);
    assertEquals('Notes', result.files[1]!.title);
    assertEquals(
        'application/vnd.google-apps.document', result.files[1]!.mimeType);
    assertEquals(0, result.files[1]!.sizeBytes);
    assertUndefined(result.files[1]!.resourceKey);
  }

  async testShowDrivePickerCancel() {
    assertDefined(this.host.showDrivePicker);
    const result = await this.host.showDrivePicker({
      accountId: this.testParams,
    });
    assertUndefined(result.errorReason);
    assertDefined(result.files);
    assertEquals(0, result.files.length);
  }

  async testShowDrivePickerCancelledOnPanelClose() {
    assertDefined(this.host.showDrivePicker);
    const resultPromise = this.host.showDrivePicker({
      accountId: this.testParams,
    });
    await this.advanceToNextStep();
    const result = await resultPromise;
    assertUndefined(result.errorReason);
    assertDefined(result.files);
    assertEquals(0, result.files.length);
  }

  async testShowDrivePickerAlreadyActive() {
    assertDefined(this.host.showDrivePicker);
    const firstPromise = this.host.showDrivePicker({
      accountId: this.testParams,
    });
    await this.advanceToNextStep();

    const secondResult = await this.host.showDrivePicker({
      accountId: this.testParams,
    });
    assertUndefined(secondResult.files);
    assertEquals(
        DrivePickerErrorReason.ALREADY_ACTIVE, secondResult.errorReason);

    await this.advanceToNextStep();
    const firstResult = await firstPromise;
    assertUndefined(firstResult.errorReason);
    assertDefined(firstResult.files);
    assertEquals(1, firstResult.files.length);
    assertEquals('file-1', firstResult.files[0]!.id);
  }

  async testShowDrivePickerAccountMismatch() {
    assertDefined(this.host.showDrivePicker);
    const result = await this.host.showDrivePicker({
      accountId: 'not-the-primary-account',
    });
    assertUndefined(result.files);
    assertEquals(DrivePickerErrorReason.ACCOUNT_MISMATCH, result.errorReason);
  }

  async testShowDrivePickerDisabledByPolicy() {
    assertEquals(
        FileUploadPolicyState.DISABLED,
        this.host.getFileUploadAllowedCapability?.().getCurrentValue());
    assertDefined(this.host.showDrivePicker);
    const result = await this.host.showDrivePicker({
      accountId: 'test-account',
    });
    assertUndefined(result.files);
    assertEquals(DrivePickerErrorReason.DISABLED_BY_POLICY, result.errorReason);
  }
}

class GlicApiDrivePickerDisabledBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testShowDrivePickerUnavailable() {
    assertDefined(this.host.getHostCapabilities);
    assertFalse(
        this.host.getHostCapabilities().has(HostCapability.DRIVE_PICKER));
    assertUndefined(this.host.showDrivePicker);
  }
}

testMain([
  GlicApiDrivePickerBrowserTest,
  GlicApiDrivePickerDisabledBrowserTest,
]);
