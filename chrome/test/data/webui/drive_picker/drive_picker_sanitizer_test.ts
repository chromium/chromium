// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DrivePickerSanitizer, SanitizationError} from 'chrome://drive-picker-host/untrusted/sanitizer.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('DrivePickerSanitizerTest', function() {
  const pickerKeys = {
    ID: 'id',
    MIME_TYPE: 'mimeType',
    NAME: 'name',
    TYPE: 'type',
    SIZE_BYTES: 'sizeBytes',
    RESOURCE_KEY: 'resourceKey',
    THUMBNAIL_URL: 'thumbnailUrl',
  };
  const allowedTypes = new Set(['document', 'photo', 'video']);

  test('SanitizesValidDocument', function() {
    const doc = {
      id: 'valid-id_123',
      mimeType: 'application/pdf',
      name: 'test.pdf',
      type: 'document',
      sizeBytes: 100,
      resourceKey: 'valid-key',
      thumbnailUrl:
          'https://lh3.googleusercontent.com/drive-storage/AJQWtBOI_xcNPWAVolLfcNDZGWus_ELL8GP-ZYLdkxOcPlUxsVOMr5jn-i261zxtYVcpA0kZePsVWT2Ghrxdg03aoJYX7t9vlc4ojecNyW7QNrP6muY9-wvmO77SsiXHrwnU2GbRCt2V9NDv62u2R96rpvI=s220',
    };

    const sanitized = DrivePickerSanitizer.sanitizeDocument(
        doc as Record<string, unknown>, pickerKeys, allowedTypes);

    assertEquals('valid-id_123', sanitized.id);
    assertEquals('application/pdf', sanitized.mimeType);
    assertEquals('test.pdf', sanitized.name);
    assertEquals('document', sanitized.type);
    assertEquals(100n, sanitized.sizeBytes);
    assertEquals('valid-key', sanitized.resourceKey);
    // Non-photo types should have null thumbnailUrl even if provided.
    assertEquals(null, sanitized.thumbnailUrl);
  });

  test('SanitizesValidPhotoWithThumbnail', function() {
    const validUrl =
        'https://lh3.googleusercontent.com/drive-storage/AJQWtBOI_xcNPWAVolLfcNDZGWus_ELL8GP-ZYLdkxOcPlUxsVOMr5jn-i261zxtYVcpA0kZePsVWT2Ghrxdg03aoJYX7t9vlc4ojecNyW7QNrP6muY9-wvmO77SsiXHrwnU2GbRCt2V9NDv62u2R96rpvI=s220';
    const doc = {
      id: 'valid-id',
      mimeType: 'image/jpeg',
      name: 'test.jpg',
      type: 'photo',
      sizeBytes: 100,
      thumbnailUrl: validUrl,
    };

    const sanitized = DrivePickerSanitizer.sanitizeDocument(
        doc as Record<string, unknown>, pickerKeys, allowedTypes);

    assertEquals(validUrl, sanitized.thumbnailUrl);
  });

  test('SanitizesPhotoWithInvalidThumbnail', function() {
    const invalidUrl = 'https://malicious.com/not-drive-storage/AJQWtBOI';
    const doc = {
      id: 'valid-id',
      mimeType: 'image/jpeg',
      name: 'test.jpg',
      type: 'photo',
      sizeBytes: 100,
      thumbnailUrl: invalidUrl,
    };

    const sanitized = DrivePickerSanitizer.sanitizeDocument(
        doc as Record<string, unknown>, pickerKeys, allowedTypes);

    assertEquals(null, sanitized.thumbnailUrl);
  });

  test('SanitizesValidDocumentWithStringSize', function() {
    const doc = {
      id: 'valid-id',
      mimeType: 'application/pdf',
      name: 'test.pdf',
      type: 'document',
      sizeBytes: '1234567890123456789',
    };

    const sanitized = DrivePickerSanitizer.sanitizeDocument(
        doc as Record<string, unknown>, pickerKeys, allowedTypes);

    assertEquals(1234567890123456789n, sanitized.sizeBytes);
  });

  test('ThrowsOnInvalidId', function() {
    const doc = {
      id: 'invalid id with spaces',
      mimeType: 'application/pdf',
      name: 'test.pdf',
      type: 'document',
      sizeBytes: 100,
    };

    try {
      DrivePickerSanitizer.sanitizeDocument(
          doc as Record<string, unknown>, pickerKeys, allowedTypes);
      assertTrue(false, 'Should have thrown');
    } catch (e) {
      assertTrue(e instanceof Error);
      assertEquals(String(SanitizationError.INVALID_FILE_ID), e.message);
    }
  });

  test('ThrowsOnUnsupportedType', function() {
    const doc = {
      id: 'valid-id',
      mimeType: 'application/pdf',
      name: 'test.pdf',
      type: 'unsupported',
      sizeBytes: 100,
    };

    try {
      DrivePickerSanitizer.sanitizeDocument(
          doc as Record<string, unknown>, pickerKeys, allowedTypes);
      assertTrue(false, 'Should have thrown');
    } catch (e) {
      assertTrue(e instanceof Error);
      assertEquals(String(SanitizationError.UNSUPPORTED_FILE_TYPE), e.message);
    }
  });

  test('ThrowsOnInvalidSize', function() {
    const doc = {
      id: 'valid-id',
      mimeType: 'application/pdf',
      name: 'test.pdf',
      type: 'document',
      sizeBytes: -1,
    };

    try {
      DrivePickerSanitizer.sanitizeDocument(
          doc as Record<string, unknown>, pickerKeys, allowedTypes);
      assertTrue(false, 'Should have thrown');
    } catch (e) {
      assertTrue(e instanceof Error);
      assertEquals(String(SanitizationError.INVALID_FILE_SIZE), e.message);
    }
  });

  test('SanitizesInvalidResourceKey', function() {
    const doc = {
      id: 'valid-id',
      mimeType: 'application/pdf',
      name: 'test.pdf',
      type: 'document',
      sizeBytes: 100,
      resourceKey: 'invalid key',
    };

    const sanitized = DrivePickerSanitizer.sanitizeDocument(
        doc as Record<string, unknown>, pickerKeys, allowedTypes);

    assertEquals(null, sanitized.resourceKey);
  });
});
