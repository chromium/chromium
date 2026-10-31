// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.MimeTypeUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.charset.StandardCharsets;

/**
 * Identifies an image payload's container format from its leading bytes.
 *
 * <p>Attachment bytes are ultimately decoded in the browser process by Blink's image decoders (see
 * {@code third_party/blink/renderer/platform/image-decoders/image_decoder.cc}), which select a
 * decoder purely by signature and support JPEG, PNG, GIF, WebP, BMP, ICO and AVIF. Anything else
 * (notably HEIF/HEIC and TIFF-based DNG, both of which Android's platform decoder <em>can</em>
 * handle) yields a null bitmap and fails the upload. Content providers also sometimes misreport the
 * mime type of such files (e.g. {@code image/jpeg} for a cloud-prepared HEIC), so the declared mime
 * type is not a reliable signal; the bytes are.
 */
@NullMarked
final class ImageFormatSniffer {
    /** Enough bytes to inspect every signature handled here, including an ISOBMFF major brand. */
    private static final int MIN_SNIFF_LENGTH = 12;

    /** Offset of the first compatible brand: box size, "ftyp", major brand, minor version. */
    private static final int FTYP_COMPATIBLE_BRANDS_OFFSET = 16;

    /** Upper bound on the ftyp bytes scanned for compatible brands. */
    private static final int MAX_FTYP_SCAN_LENGTH = 64;

    /** Length of an ISOBMFF brand (a four-character code). */
    private static final int BRAND_LENGTH = 4;

    /** Container formats this sniffer can identify. Values are recorded in UMA; do not reorder. */
    // LINT.IfChange(FuseboxSniffedImageFormat)
    @IntDef({
        ImageFormat.UNKNOWN,
        ImageFormat.JPEG,
        ImageFormat.PNG,
        ImageFormat.GIF,
        ImageFormat.WEBP,
        ImageFormat.BMP,
        ImageFormat.ICO,
        ImageFormat.AVIF,
        ImageFormat.HEIF,
        ImageFormat.TIFF,
        ImageFormat.JXL,
        ImageFormat.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ImageFormat {
        /** No recognised image signature (includes text formats such as SVG). */
        int UNKNOWN = 0;

        int JPEG = 1;
        int PNG = 2;
        int GIF = 3;
        int WEBP = 4;
        int BMP = 5;
        int ICO = 6;

        /** ISOBMFF with an AVIF brand. Decodable by Blink. */
        int AVIF = 7;

        /** ISOBMFF with a HEIF/HEIC brand. Not decodable by Blink. */
        int HEIF = 8;

        /** TIFF container, which includes DNG raw captures. Not decodable by Blink. */
        int TIFF = 9;

        /** JPEG XL. Only decodable by Blink behind a build flag and feature. */
        int JXL = 10;

        int COUNT = 11;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:FuseboxSniffedImageFormat)

    private ImageFormatSniffer() {}

    /**
     * Identifies the container format of {@code data} from its signature.
     *
     * @param data The leading bytes of the payload (the whole payload is fine).
     * @return The detected {@link ImageFormat}, or {@link ImageFormat#UNKNOWN}.
     */
    static @ImageFormat int sniff(byte[] data) {
        if (data.length < MIN_SNIFF_LENGTH) return ImageFormat.UNKNOWN;

        // JPEG: FF D8 FF
        if (u8(data, 0) == 0xFF && u8(data, 1) == 0xD8 && u8(data, 2) == 0xFF) {
            return ImageFormat.JPEG;
        }
        // PNG: 89 'P' 'N' 'G' 0D 0A 1A 0A
        if (u8(data, 0) == 0x89
                && startsWithAscii(data, 1, "PNG")
                && u8(data, 4) == 0x0D
                && u8(data, 5) == 0x0A
                && u8(data, 6) == 0x1A
                && u8(data, 7) == 0x0A) {
            return ImageFormat.PNG;
        }
        // GIF: "GIF87a" / "GIF89a"
        if (startsWithAscii(data, 0, "GIF87a") || startsWithAscii(data, 0, "GIF89a")) {
            return ImageFormat.GIF;
        }
        // WebP: "RIFF" .... "WEBP"
        if (startsWithAscii(data, 0, "RIFF") && startsWithAscii(data, 8, "WEBP")) {
            return ImageFormat.WEBP;
        }
        // BMP: "BM"
        if (startsWithAscii(data, 0, "BM")) {
            return ImageFormat.BMP;
        }
        // ICO: 00 00 01 00, CUR: 00 00 02 00
        if (u8(data, 0) == 0x00
                && u8(data, 1) == 0x00
                && (u8(data, 2) == 0x01 || u8(data, 2) == 0x02)
                && u8(data, 3) == 0x00) {
            return ImageFormat.ICO;
        }
        // TIFF (incl. DNG): "II*\0" little-endian or "MM\0*" big-endian.
        if ((startsWithAscii(data, 0, "II") && u8(data, 2) == 0x2A && u8(data, 3) == 0x00)
                || (startsWithAscii(data, 0, "MM") && u8(data, 2) == 0x00 && u8(data, 3) == 0x2A)) {
            return ImageFormat.TIFF;
        }
        // JPEG XL: bare codestream FF 0A, or container 00 00 00 0C 'J' 'X' 'L' ' ' 0D 0A 87 0A
        if ((u8(data, 0) == 0xFF && u8(data, 1) == 0x0A)
                || (u8(data, 0) == 0x00
                        && u8(data, 1) == 0x00
                        && u8(data, 2) == 0x00
                        && u8(data, 3) == 0x0C
                        && startsWithAscii(data, 4, "JXL ")
                        && u8(data, 8) == 0x0D
                        && u8(data, 9) == 0x0A
                        && u8(data, 10) == 0x87
                        && u8(data, 11) == 0x0A)) {
            return ImageFormat.JXL;
        }
        // ISOBMFF: bytes 4..7 spell "ftyp"; bytes 8..11 carry the major brand.
        if (startsWithAscii(data, 4, "ftyp")) {
            String brand = new String(data, 8, BRAND_LENGTH, StandardCharsets.US_ASCII);
            return switch (brand) {
                case "avif", "avis" -> ImageFormat.AVIF;
                // Generic MIAF brands: AVIF files often use these as the major brand.
                case "mif1", "msf1" ->
                        hasCompatibleBrand(data, "avif", "avis")
                                ? ImageFormat.AVIF
                                : ImageFormat.HEIF;
                case "heic", "heix", "heim", "heis", "hevc", "hevx", "hevm", "hevs" ->
                        ImageFormat.HEIF;
                default -> ImageFormat.UNKNOWN;
            };
        }
        return ImageFormat.UNKNOWN;
    }

    /** Returns whether the leading ftyp box lists either brand among its compatible brands. */
    private static boolean hasCompatibleBrand(byte[] data, String brand, String otherBrand) {
        // The box size is an unsigned big-endian 32-bit value; read it as a long so it can't go
        // negative. 0 means the box runs to the end of the data; 1 means a 64-bit size follows,
        // which moves the brands and is not expected for ftyp.
        long boxSize =
                ((long) u8(data, 0) << 24) | (u8(data, 1) << 16) | (u8(data, 2) << 8) | u8(data, 3);
        if (boxSize == 1) return false;
        long boxEnd = boxSize == 0 ? data.length : Math.min(boxSize, data.length);
        int end = (int) Math.min(boxEnd, MAX_FTYP_SCAN_LENGTH);
        for (int offset = FTYP_COMPATIBLE_BRANDS_OFFSET;
                offset + BRAND_LENGTH <= end;
                offset += BRAND_LENGTH) {
            if (startsWithAscii(data, offset, brand) || startsWithAscii(data, offset, otherBrand)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Returns whether Blink's image decoders can decode {@code format}. Mirrors the signature
     * dispatch in {@code blink::ImageDecoder::Create}.
     */
    static boolean isBlinkDecodable(@ImageFormat int format) {
        return switch (format) {
            case ImageFormat.JPEG,
                    ImageFormat.PNG,
                    ImageFormat.GIF,
                    ImageFormat.WEBP,
                    ImageFormat.BMP,
                    ImageFormat.ICO,
                    ImageFormat.AVIF ->
                    true;
            // JXL is only decodable behind a build flag and a runtime feature; treat it as
            // unsupported so that it is transcoded (or fails locally) rather than failing in
            // native.
            default -> false;
        };
    }

    /**
     * Returns whether an attachment must be re-encoded on the client before upload.
     *
     * <p>True when the bytes are positively identified as a format Blink cannot decode, or when the
     * provider declared an image but the signature is unrecognised. In the latter case Blink would
     * fail anyway, so attempting the platform decoder first can only help.
     *
     * @param declaredMimeType The mime type reported by the content provider.
     * @param format The format sniffed from the bytes.
     */
    static boolean requiresClientTranscode(String declaredMimeType, @ImageFormat int format) {
        if (format != ImageFormat.UNKNOWN) return !isBlinkDecodable(format);
        return MimeTypeUtils.getTypeFromMimeType(declaredMimeType) == MimeTypeUtils.Type.IMAGE;
    }

    /**
     * Returns whether the declared mime type disagrees with the sniffed format. Only formats with
     * an unambiguous mime mapping are compared; anything else is treated as consistent.
     */
    static boolean declaredMimeTypeMismatches(String declaredMimeType, @ImageFormat int format) {
        @ImageFormat int declared = formatForMimeType(declaredMimeType);
        return declared != ImageFormat.UNKNOWN
                && format != ImageFormat.UNKNOWN
                && declared != format;
    }

    /** Maps a mime type to the {@link ImageFormat} it nominally denotes. */
    static @ImageFormat int formatForMimeType(String mimeType) {
        return switch (mimeType) {
            case MimeTypeUtils.IMAGE_JPEG_MIME_TYPE, MimeTypeUtils.IMAGE_JPG_MIME_TYPE ->
                    ImageFormat.JPEG;
            case MimeTypeUtils.IMAGE_PNG_MIME_TYPE -> ImageFormat.PNG;
            case "image/gif" -> ImageFormat.GIF;
            case "image/webp" -> ImageFormat.WEBP;
            case "image/bmp", "image/x-ms-bmp" -> ImageFormat.BMP;
            case "image/x-icon", "image/vnd.microsoft.icon" -> ImageFormat.ICO;
            case "image/avif" -> ImageFormat.AVIF;
            case "image/heic", "image/heif", "image/heic-sequence", "image/heif-sequence" ->
                    ImageFormat.HEIF;
            case "image/tiff", "image/x-adobe-dng" -> ImageFormat.TIFF;
            case "image/jxl" -> ImageFormat.JXL;
            default -> ImageFormat.UNKNOWN;
        };
    }

    private static int u8(byte[] data, int index) {
        return data[index] & 0xFF;
    }

    private static boolean startsWithAscii(byte[] data, int offset, String ascii) {
        if (data.length < offset + ascii.length()) return false;
        for (int i = 0; i < ascii.length(); i++) {
            if (data[offset + i] != (byte) ascii.charAt(i)) return false;
        }
        return true;
    }
}
