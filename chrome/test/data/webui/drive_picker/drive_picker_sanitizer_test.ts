// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DrivePickerSanitizer, SanitizationError} from 'chrome://drive-picker-host/untrusted/sanitizer.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('DrivePickerSanitizerTest', function() {
  const pickerKeys = {
    ID: 'id',
    MIME_TYPE: 'mimeType',
    NAME: 'name',
    TYPE: 'type',
    SIZE_BYTES: 'sizeBytes',
    RESOURCE_KEY: 'resourceKey',
    THUMBNAIL_URL: 'thumbnailUrl',
    ICON_URL: 'iconUrl',
  };
  const allowedTypes = new Set(['document', 'file', 'photo', 'video']);

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
        doc as Record<string, unknown>, pickerKeys, doc.thumbnailUrl,
        allowedTypes);

    assertEquals('valid-id_123', sanitized.id);
    assertEquals('application/pdf', sanitized.mimeType);
    assertEquals('test.pdf', sanitized.name);
    assertEquals('document', sanitized.type);
    assertEquals(100n, sanitized.sizeBytes);
    assertEquals('valid-key', sanitized.resourceKey);
    // Non-photo types should have null thumbnailUrl even if provided.
    assertEquals(null, sanitized.thumbnailUrl);
  });

  test('SanitizesValidFile', function() {
    const doc = {
      id: 'valid-id_123',
      mimeType: 'application/zip',
      name: 'test.zip',
      type: 'file',
      sizeBytes: 100,
      resourceKey: 'valid-key',
      thumbnailUrl:
          'https://lh3.googleusercontent.com/drive-storage/AJQWtBOI_xcNPWAVolLfcNDZGWus_ELL8GP-ZYLdkxOcPlUxsVOMr5jn-i261zxtYVcpA0kZePsVWT2Ghrxdg03aoJYX7t9vlc4ojecNyW7QNrP6muY9-wvmO77SsiXHrwnU2GbRCt2V9NDv62u2R96rpvI=s220',
    };

    const sanitized = DrivePickerSanitizer.sanitizeDocument(
        doc as Record<string, unknown>, pickerKeys, doc.thumbnailUrl,
        allowedTypes);

    assertEquals('valid-id_123', sanitized.id);
    assertEquals('application/zip', sanitized.mimeType);
    assertEquals('test.zip', sanitized.name);
    assertEquals('file', sanitized.type);
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
        doc as Record<string, unknown>, pickerKeys, validUrl, allowedTypes);

    assertEquals(validUrl, sanitized.thumbnailUrl);
  });

  test('SanitizesValidPhotoWithDThumbnail', function() {
    const validUrl =
        'https://lh3.googleusercontent.com/d/1aBcD2eFgH3iJkL4mN-oPqR5sTuvWxYz0=s220';
    const doc = {
      id: 'valid-id',
      mimeType: 'image/jpeg',
      name: 'test.jpg',
      type: 'photo',
      sizeBytes: 100,
      thumbnailUrl: validUrl,
    };

    const sanitized = DrivePickerSanitizer.sanitizeDocument(
        doc as Record<string, unknown>, pickerKeys, validUrl, allowedTypes);

    assertEquals(validUrl, sanitized.thumbnailUrl);
  });

  test('SanitizesValidVideoWithThumbnail', function() {
    const validUrl =
        'https://lh3.googleusercontent.com/d/1aBcD2eFgH3iJkL4mN-oPqR5sTuvWxYz0=s220';
    const doc = {
      id: 'valid-id',
      mimeType: 'video/mp4',
      name: 'test.mp4',
      type: 'video',
      sizeBytes: 100,
      thumbnailUrl: validUrl,
    };

    const sanitized = DrivePickerSanitizer.sanitizeDocument(
        doc as Record<string, unknown>, pickerKeys, validUrl, allowedTypes);

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
        doc as Record<string, unknown>, pickerKeys, invalidUrl, allowedTypes);

    assertEquals(null, sanitized.thumbnailUrl);
  });

  test('SanitizeDriveIconUrl', function() {
    // Valid hosts and paths.
    const validUrls = [
      'https://lh3.googleusercontent.com/16/hype/pdf',
      'https://lh6.googleusercontent.com/16/type/image/png',
      'https://drive-thirdparty.googleusercontent.com/16/type/pdf',
    ];

    for (const url of validUrls) {
      const sanitized = DrivePickerSanitizer.sanitizeIconUrl(url);
      assertTrue(!!sanitized);
      assertEquals(url, sanitized);
    }

    // Invalid hosts.
    const invalidHosts = [
      'https://malicious.com/16/type/pdf',
      'https://lh7.googleusercontent.com/16/type/pdf',
    ];

    for (const url of invalidHosts) {
      assertEquals(null, DrivePickerSanitizer.sanitizeIconUrl(url));
    }

    // Invalid paths.
    const invalidPaths = [
      'https://lh3.googleusercontent.com/invalid/path',
      'https://lh3.googleusercontent.com/16/other/pdf',
    ];

    for (const url of invalidPaths) {
      assertEquals(null, DrivePickerSanitizer.sanitizeIconUrl(url));
    }
  });

  test('SanitizesValidDocumentWithIconUrl', function() {
    const iconUrl = 'https://lh3.googleusercontent.com/16/hype/pdf';
    const doc = {
      id: 'valid-id',
      mimeType: 'application/pdf',
      name: 'test.pdf',
      type: 'document',
      sizeBytes: 100,
      iconUrl: iconUrl,
    };

    const sanitized = DrivePickerSanitizer.sanitizeDocument(
        doc as Record<string, unknown>, pickerKeys, null, allowedTypes);

    assertTrue(!!sanitized.iconUrl);
    assertEquals(iconUrl, sanitized.iconUrl);
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
        doc as Record<string, unknown>, pickerKeys, null, allowedTypes);

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
          doc as Record<string, unknown>, pickerKeys, null, allowedTypes);
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
          doc as Record<string, unknown>, pickerKeys, null, allowedTypes);
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
          doc as Record<string, unknown>, pickerKeys, null, allowedTypes);
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
        doc as Record<string, unknown>, pickerKeys, null, allowedTypes);

    assertEquals(null, sanitized.resourceKey);
  });

  test('SanitizeThumbnailUrlHelper_ValidUrls', function() {
    const validUrls = [
      'https://lh3.googleusercontent.com/drive-storage/AJQWtBOI_xcNPWAVolLfcNDZGWus_ELL8GP',
      'https://lh6.googleusercontent.com/d/foo-bar_baz=123',
      'https://lh4.googleusercontent.com/d/foo-bar_baz=123',
      'https://lh5.googleusercontent.com/rd-d/foo-bar_baz=123',
      'https://drive.google.com/thumbnail?sz=w320&id=1aBcD2eFgH3iJkL4m_aB-',
    ];

    for (const url of validUrls) {
      const sanitized = DrivePickerSanitizer.sanitizeThumbnailUrl(url);
      assertTrue(!!sanitized, `Expected URL to be valid: ${url}`);
      assertEquals(url, sanitized, `Expected URL to be valid: ${url}`);
    }
  });

  test('SanitizeThumbnailUrlHelper_InvalidUrls', function() {
    const invalidUrls = [
      null,
      undefined,
      123,
      '',
      // Wrong scheme
      'http://lh3.googleusercontent.com/drive-storage/AJQWt',
      // Wrong host
      'https://lh7.googleusercontent.com/drive-storage/AJQWt',
      'https://malicious.com/drive-storage/AJQWt',
      // Path traversal or invalid chars
      'https://lh3.googleusercontent.com/drive-storage/../AJQWt',
      'https://lh3.googleusercontent.com/drive-storage/foo^bar',
      // Invalid drive.google.com paths/params
      'https://drive.google.com/thumbnail',
      'https://drive.google.com/thumbnail_abc',
      'https://drive.google.com/thumbnail/abc',
      'https://drive.google.com/thumbnail?id=1234&sz=w200-h200',
      'https://drive.google.com/file/d/1aBcD2eFgH3iJkL4m_aB=/view',
      'https://drive.google.com/file/d/1aBcD2eFgH3iJkL4m_aB=/preview',
      'https://drive.google.com/file/d/abc/edit',
      'https://drive.google.com/file/d//view',
    ];

    for (const url of invalidUrls) {
      assertEquals(
          null, DrivePickerSanitizer.sanitizeThumbnailUrl(url),
          `Expected URL to be invalid: ${url}`);
    }
  });

  test('IsValidGoogleDriveThumbnailUrl', function() {
    // Valid drive thumbnail URL.
    assertTrue(DrivePickerSanitizer.isValidGoogleDriveThumbnailUrl(
        'https://drive.google.com/thumbnail?sz=w320&id=1aBcD2eFgH3iJkL4m_aB-'));

    // Invalid: wrong base URL.
    assertFalse(DrivePickerSanitizer.isValidGoogleDriveThumbnailUrl(
        'https://malicious.com/thumbnail?sz=w320&id=123'));

    // Invalid: lacks size param or wrong size param.
    assertFalse(DrivePickerSanitizer.isValidGoogleDriveThumbnailUrl(
        'https://drive.google.com/thumbnail?id=123'));
    assertFalse(DrivePickerSanitizer.isValidGoogleDriveThumbnailUrl(
        'https://drive.google.com/thumbnail?sz=w640&id=123'));

    // Invalid: wrong parameter order.
    assertFalse(DrivePickerSanitizer.isValidGoogleDriveThumbnailUrl(
        'https://drive.google.com/thumbnail?id=123&sz=w320'));

    // Invalid: unsafe characters in ID.
    assertFalse(DrivePickerSanitizer.isValidGoogleDriveThumbnailUrl(
        'https://drive.google.com/thumbnail?sz=w320&id=invalid^chars'));

    // Invalid: trailing data or parameters for ID.
    assertFalse(DrivePickerSanitizer.isValidGoogleDriveThumbnailUrl(
        'https://drive.google.com/thumbnail?sz=w320&id=123/edit'));
    assertFalse(DrivePickerSanitizer.isValidGoogleDriveThumbnailUrl(
        'https://drive.google.com/thumbnail?sz=w320&id=123&foo=bar'));
  });

  test('IsValidUserContentThumbnailUrl', function() {
    // Valid lh thumbnail URLs.
    assertTrue(DrivePickerSanitizer.isValidUserContentThumbnailUrl(
        'https://lh3.googleusercontent.com/drive-storage/AJQWtBOI_xcNPWAVolLfcNDZGWus_ELL8GP'));
    assertTrue(DrivePickerSanitizer.isValidUserContentThumbnailUrl(
        'https://lh6.googleusercontent.com/d/foo-bar_baz=123'));
    assertTrue(DrivePickerSanitizer.isValidUserContentThumbnailUrl(
        'https://lh5.googleusercontent.com/rd-d/foo-bar_baz=123'));

    // Invalid scheme.
    assertFalse(DrivePickerSanitizer.isValidUserContentThumbnailUrl(
        'http://lh3.googleusercontent.com/drive-storage/AJQWt'));

    // Invalid host.
    assertFalse(DrivePickerSanitizer.isValidUserContentThumbnailUrl(
        'https://lh7.googleusercontent.com/drive-storage/AJQWt'));
    assertFalse(DrivePickerSanitizer.isValidUserContentThumbnailUrl(
        'https://malicious.com/drive-storage/AJQWt'));

    // Invalid path.
    assertFalse(DrivePickerSanitizer.isValidUserContentThumbnailUrl(
        'https://lh3.googleusercontent.com/other-path/AJQWt'));

    // Path traversal / unsafe characters.
    assertFalse(DrivePickerSanitizer.isValidUserContentThumbnailUrl(
        'https://lh3.googleusercontent.com/drive-storage/../AJQWt'));
    assertFalse(DrivePickerSanitizer.isValidUserContentThumbnailUrl(
        'https://lh3.googleusercontent.com/drive-storage/foo^bar'));
  });
});
