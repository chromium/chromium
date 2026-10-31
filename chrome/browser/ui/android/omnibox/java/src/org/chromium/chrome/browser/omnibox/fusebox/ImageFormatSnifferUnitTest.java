// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.fusebox.ImageFormatSniffer.ImageFormat;
import org.chromium.ui.base.MimeTypeUtils;

import java.nio.charset.StandardCharsets;

/** Unit tests for {@link ImageFormatSniffer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ImageFormatSnifferUnitTest {
    /** Minimal JPEG prefix: SOI + APP0 marker, padded. */
    static final byte[] JPEG =
            padded(new byte[] {(byte) 0xFF, (byte) 0xD8, (byte) 0xFF, (byte) 0xE0});

    /** Standard 8-byte PNG signature, padded. */
    static final byte[] PNG =
            padded(new byte[] {(byte) 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A});

    static final byte[] GIF87 = padded(ascii("GIF87a"));
    static final byte[] GIF89 = padded(ascii("GIF89a"));
    static final byte[] WEBP = riff("WEBP");
    static final byte[] BMP = padded(ascii("BM"));
    static final byte[] ICO = padded(new byte[] {0x00, 0x00, 0x01, 0x00});
    static final byte[] CUR = padded(new byte[] {0x00, 0x00, 0x02, 0x00});
    static final byte[] TIFF_LE = padded(new byte[] {'I', 'I', 0x2A, 0x00});
    static final byte[] TIFF_BE = padded(new byte[] {'M', 'M', 0x00, 0x2A});
    static final byte[] JXL_CODESTREAM = padded(new byte[] {(byte) 0xFF, 0x0A});
    static final byte[] JXL_CONTAINER =
            padded(
                    new byte[] {
                        0x00, 0x00, 0x00, 0x0C, 'J', 'X', 'L', ' ', 0x0D, 0x0A, (byte) 0x87, 0x0A
                    });

    @Test
    public void testSniff_rasterFormats() {
        assertEquals(ImageFormat.JPEG, ImageFormatSniffer.sniff(JPEG));
        assertEquals(ImageFormat.PNG, ImageFormatSniffer.sniff(PNG));
        assertEquals(ImageFormat.GIF, ImageFormatSniffer.sniff(GIF87));
        assertEquals(ImageFormat.GIF, ImageFormatSniffer.sniff(GIF89));
        assertEquals(ImageFormat.WEBP, ImageFormatSniffer.sniff(WEBP));
        assertEquals(ImageFormat.BMP, ImageFormatSniffer.sniff(BMP));
        assertEquals(ImageFormat.ICO, ImageFormatSniffer.sniff(ICO));
        assertEquals(ImageFormat.ICO, ImageFormatSniffer.sniff(CUR));
    }

    @Test
    public void testSniff_tiffAndDng() {
        assertEquals(ImageFormat.TIFF, ImageFormatSniffer.sniff(TIFF_LE));
        assertEquals(ImageFormat.TIFF, ImageFormatSniffer.sniff(TIFF_BE));
    }

    @Test
    public void testSniff_jpegXl() {
        assertEquals(ImageFormat.JXL, ImageFormatSniffer.sniff(JXL_CODESTREAM));
        assertEquals(ImageFormat.JXL, ImageFormatSniffer.sniff(JXL_CONTAINER));
    }

    @Test
    public void testSniff_isobmffBrands() {
        for (String brand :
                new String[] {
                    "heic", "heix", "heim", "heis", "hevc", "hevx", "hevm", "hevs", "mif1", "msf1"
                }) {
            assertEquals(
                    "brand " + brand, ImageFormat.HEIF, ImageFormatSniffer.sniff(isobmff(brand)));
        }
        assertEquals(ImageFormat.AVIF, ImageFormatSniffer.sniff(isobmff("avif")));
        assertEquals(ImageFormat.AVIF, ImageFormatSniffer.sniff(isobmff("avis")));
        // MP4 video shares the container but is not an image.
        assertEquals(ImageFormat.UNKNOWN, ImageFormatSniffer.sniff(isobmff("isom")));
        assertEquals(ImageFormat.UNKNOWN, ImageFormatSniffer.sniff(isobmff("mp42")));
    }

    @Test
    public void testSniff_isobmffMiafCompatibleBrands() {
        // AVIF encoders commonly write mif1/msf1 as the major brand with avif/avis compatible.
        assertEquals(ImageFormat.AVIF, ImageFormatSniffer.sniff(isobmff("mif1", "mif1", "avif")));
        assertEquals(ImageFormat.AVIF, ImageFormatSniffer.sniff(isobmff("msf1", "msf1", "avis")));
        assertEquals(ImageFormat.HEIF, ImageFormatSniffer.sniff(isobmff("mif1", "mif1", "heic")));

        // Box size 0 means the ftyp box extends to the end of the buffer.
        byte[] sizeToEnd = isobmff("mif1", "avif");
        sizeToEnd[3] = 0;
        assertEquals(ImageFormat.AVIF, ImageFormatSniffer.sniff(sizeToEnd));

        // Box size 1 means a 64-bit extended size follows, shifting compatible brands.
        byte[] extendedSize = isobmff("mif1", "avif");
        extendedSize[3] = 1;
        assertEquals(ImageFormat.HEIF, ImageFormatSniffer.sniff(extendedSize));

        // Bytes past the ftyp box's declared size belong to subsequent boxes and are ignored.
        byte[] trailingBox = isobmff("mif1", "mif1", "avif");
        trailingBox[3] = 20; // ftyp ends after the first compatible brand ("mif1")
        assertEquals(ImageFormat.HEIF, ImageFormatSniffer.sniff(trailingBox));
    }

    @Test
    public void testSniff_unknownAndShortInputs() {
        assertEquals(ImageFormat.UNKNOWN, ImageFormatSniffer.sniff(new byte[] {}));
        assertEquals(ImageFormat.UNKNOWN, ImageFormatSniffer.sniff(new byte[] {1, 2, 3}));
        // A JPEG SOI that is shorter than the minimum sniff length is still unknown.
        assertEquals(
                ImageFormat.UNKNOWN,
                ImageFormatSniffer.sniff(new byte[] {(byte) 0xFF, (byte) 0xD8, (byte) 0xFF}));
        assertEquals(ImageFormat.UNKNOWN, ImageFormatSniffer.sniff(padded(ascii("<svg xmlns"))));
        assertEquals(ImageFormat.UNKNOWN, ImageFormatSniffer.sniff(padded(ascii("%PDF-1.7"))));
        // RIFF that is not WebP (e.g. WAV).
        assertEquals(ImageFormat.UNKNOWN, ImageFormatSniffer.sniff(riff("WAVE")));
    }

    @Test
    public void testIsBlinkDecodable() {
        assertTrue(ImageFormatSniffer.isBlinkDecodable(ImageFormat.JPEG));
        assertTrue(ImageFormatSniffer.isBlinkDecodable(ImageFormat.PNG));
        assertTrue(ImageFormatSniffer.isBlinkDecodable(ImageFormat.GIF));
        assertTrue(ImageFormatSniffer.isBlinkDecodable(ImageFormat.WEBP));
        assertTrue(ImageFormatSniffer.isBlinkDecodable(ImageFormat.BMP));
        assertTrue(ImageFormatSniffer.isBlinkDecodable(ImageFormat.ICO));
        assertTrue(ImageFormatSniffer.isBlinkDecodable(ImageFormat.AVIF));

        assertFalse(ImageFormatSniffer.isBlinkDecodable(ImageFormat.HEIF));
        assertFalse(ImageFormatSniffer.isBlinkDecodable(ImageFormat.TIFF));
        assertFalse(ImageFormatSniffer.isBlinkDecodable(ImageFormat.JXL));
        assertFalse(ImageFormatSniffer.isBlinkDecodable(ImageFormat.UNKNOWN));
    }

    @Test
    public void testRequiresClientTranscode_knownFormatIgnoresDeclaredMime() {
        // Blink-decodable bytes never need transcoding, whatever the provider claimed.
        assertFalse(
                ImageFormatSniffer.requiresClientTranscode(
                        MimeTypeUtils.IMAGE_JPEG_MIME_TYPE, ImageFormat.JPEG));
        assertFalse(
                ImageFormatSniffer.requiresClientTranscode(
                        "application/octet-stream", ImageFormat.PNG));
        assertFalse(ImageFormatSniffer.requiresClientTranscode("image/heic", ImageFormat.JPEG));

        // Non-Blink-decodable bytes always need transcoding, whatever the provider claimed.
        assertTrue(ImageFormatSniffer.requiresClientTranscode("image/heic", ImageFormat.HEIF));
        assertTrue(
                ImageFormatSniffer.requiresClientTranscode(
                        MimeTypeUtils.IMAGE_JPEG_MIME_TYPE, ImageFormat.HEIF));
        assertTrue(
                ImageFormatSniffer.requiresClientTranscode(
                        "application/octet-stream", ImageFormat.HEIF));
        assertTrue(
                ImageFormatSniffer.requiresClientTranscode(
                        "application/octet-stream", ImageFormat.TIFF));
        assertTrue(ImageFormatSniffer.requiresClientTranscode("image/jxl", ImageFormat.JXL));
    }

    @Test
    public void testRequiresClientTranscode_unknownFormatFallsBackToDeclaredMime() {
        // Unrecognised bytes with an image mime type: Blink would fail anyway, so try the
        // platform decoder.
        assertTrue(
                ImageFormatSniffer.requiresClientTranscode(
                        MimeTypeUtils.IMAGE_JPEG_MIME_TYPE, ImageFormat.UNKNOWN));
        assertTrue(
                ImageFormatSniffer.requiresClientTranscode("image/svg+xml", ImageFormat.UNKNOWN));
        // Unrecognised bytes with a non-image mime type are not images; leave them alone.
        assertFalse(
                ImageFormatSniffer.requiresClientTranscode(
                        MimeTypeUtils.TEXT_PLAIN_MIME_TYPE, ImageFormat.UNKNOWN));
        assertFalse(
                ImageFormatSniffer.requiresClientTranscode(
                        "application/octet-stream", ImageFormat.UNKNOWN));
    }

    @Test
    public void testDeclaredMimeTypeMismatches() {
        assertFalse(
                ImageFormatSniffer.declaredMimeTypeMismatches(
                        MimeTypeUtils.IMAGE_JPEG_MIME_TYPE, ImageFormat.JPEG));
        assertFalse(
                ImageFormatSniffer.declaredMimeTypeMismatches(
                        MimeTypeUtils.IMAGE_JPG_MIME_TYPE, ImageFormat.JPEG));
        assertFalse(ImageFormatSniffer.declaredMimeTypeMismatches("image/heic", ImageFormat.HEIF));
        assertFalse(
                ImageFormatSniffer.declaredMimeTypeMismatches(
                        "image/x-adobe-dng", ImageFormat.TIFF));

        assertTrue(
                ImageFormatSniffer.declaredMimeTypeMismatches(
                        MimeTypeUtils.IMAGE_JPEG_MIME_TYPE, ImageFormat.HEIF));
        assertTrue(
                ImageFormatSniffer.declaredMimeTypeMismatches(
                        MimeTypeUtils.IMAGE_PNG_MIME_TYPE, ImageFormat.JPEG));

        // Either side unknown is treated as consistent.
        assertFalse(
                ImageFormatSniffer.declaredMimeTypeMismatches(
                        MimeTypeUtils.IMAGE_JPEG_MIME_TYPE, ImageFormat.UNKNOWN));
        assertFalse(
                ImageFormatSniffer.declaredMimeTypeMismatches(
                        "application/octet-stream", ImageFormat.HEIF));
        assertFalse(ImageFormatSniffer.declaredMimeTypeMismatches("image/*", ImageFormat.JPEG));
    }

    /** Builds an ISOBMFF ftyp box with the given major brand and optional compatible brands. */
    static byte[] isobmff(String majorBrand, String... compatibleBrands) {
        int boxSize = 16 + 4 * compatibleBrands.length;
        byte[] out = new byte[boxSize];
        out[0] = (byte) ((boxSize >>> 24) & 0xFF);
        out[1] = (byte) ((boxSize >>> 16) & 0xFF);
        out[2] = (byte) ((boxSize >>> 8) & 0xFF);
        out[3] = (byte) (boxSize & 0xFF);
        System.arraycopy(ascii("ftyp"), 0, out, 4, 4);
        System.arraycopy(ascii(majorBrand), 0, out, 8, 4);
        for (int i = 0; i < compatibleBrands.length; i++) {
            System.arraycopy(ascii(compatibleBrands[i]), 0, out, 16 + 4 * i, 4);
        }
        return out;
    }

    private static byte[] riff(String form) {
        byte[] out = new byte[16];
        System.arraycopy(ascii("RIFF"), 0, out, 0, 4);
        System.arraycopy(ascii(form), 0, out, 8, 4);
        return out;
    }

    private static byte[] ascii(String s) {
        return s.getBytes(StandardCharsets.US_ASCII);
    }

    /** Pads {@code prefix} with zeros so that it meets the sniffer's minimum length. */
    private static byte[] padded(byte[] prefix) {
        byte[] out = new byte[Math.max(16, prefix.length)];
        System.arraycopy(prefix, 0, out, 0, prefix.length);
        return out;
    }
}
