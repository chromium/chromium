// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapk.lib.client;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.webapk.lib.client.WebApkVerifySignature.Error;
import org.chromium.testing.local.TestDir;

import java.io.RandomAccessFile;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.KeyFactory;
import java.security.PublicKey;
import java.security.spec.X509EncodedKeySpec;

/** Unit tests for WebApkVerifySignature for Android. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkVerifySignatureTest {
    /** Elliptical Curves, Digital Signature Algorithm */
    private static final String KEY_FACTORY = "EC";

    private static final String TEST_DATA_DIR = "/webapks/";

    static PublicKey readPublicKey(String publicDER) throws Exception {
        return createPublicKey(Files.readAllBytes(Paths.get(publicDER)));
    }

    private static PublicKey createPublicKey(byte[] bytes) throws Exception {
        return KeyFactory.getInstance(KEY_FACTORY).generatePublic(new X509EncodedKeySpec(bytes));
    }

    @Test
    public void testHexToBytes() {
        byte[] empty = {};
        assertArrayEquals(empty, WebApkVerifySignature.hexToBytes(""));
        byte[] test = {(byte) 0xFF, (byte) 0xFE, 0x00, 0x01};
        assertArrayEquals(test, WebApkVerifySignature.hexToBytes("fffe0001"));
        assertArrayEquals(test, WebApkVerifySignature.hexToBytes("FFFE0001"));
        assertEquals(null, WebApkVerifySignature.hexToBytes("f")); // Odd number of nibbles.
    }

    @Test
    public void testCommentHash() {
        byte[] bytes = {(byte) 0xde, (byte) 0xca, (byte) 0xfb, (byte) 0xad};
        assertEquals(null, WebApkVerifySignature.parseCommentSignature("weapk:decafbad"));
        assertEquals(null, WebApkVerifySignature.parseCommentSignature("webapk:"));
        assertEquals(null, WebApkVerifySignature.parseCommentSignature("webapk:decafbad"));
        assertArrayEquals(
                bytes, WebApkVerifySignature.parseCommentSignature("webapk:12345:decafbad"));
        assertArrayEquals(
                bytes, WebApkVerifySignature.parseCommentSignature("XXXwebapk:0000:decafbadXXX"));
        assertArrayEquals(
                bytes, WebApkVerifySignature.parseCommentSignature("\n\nwebapk:0000:decafbad\n\n"));
        assertArrayEquals(
                bytes,
                WebApkVerifySignature.parseCommentSignature("chrome-webapk:000:decafbad\n\n"));
        assertArrayEquals(
                bytes,
                WebApkVerifySignature.parseCommentSignature(
                        "prefixed: chrome-webapk:000:decafbad :suffixed"));
    }

    static class FileResult {
        FileResult(String filename, int expect) {
            this.filename = filename;
            this.want = expect;
        }

        public final String filename;
        public final int want;
    }

    @Test
    public void testBadVerifyFiles() throws Exception {
        PublicKey pub = readPublicKey(testFilePath("public.der"));
        FileResult[] tests = {
            new FileResult("example.apk", Error.OK),
            new FileResult("java-example.apk", Error.OK),
            new FileResult("v2-signed-ok.apk", Error.OK),
            new FileResult("bad-sig.apk", Error.INCORRECT_SIGNATURE),
            new FileResult("bad-utf8-fname.apk", Error.INCORRECT_SIGNATURE),
            new FileResult("empty.apk", Error.BAD_APK),
            new FileResult("extra-field-too-large.apk", Error.OK),
            new FileResult("extra-len-too-large.apk", Error.BAD_APK),
            new FileResult("no-cd.apk", Error.BAD_APK),
            new FileResult("no-comment.apk", Error.SIGNATURE_NOT_FOUND),
            new FileResult("no-eocd.apk", Error.BAD_APK),
            new FileResult("no-lfh.apk", Error.BAD_APK),
            new FileResult("not-an.apk", Error.BAD_APK),
            new FileResult("too-many-metainf.apk", Error.TOO_MANY_META_INF_FILES),
            new FileResult("truncated.apk", Error.BAD_APK),
            new FileResult("zeros.apk", Error.BAD_APK),
            new FileResult("zeros-at-end.apk", Error.BAD_APK),
            new FileResult("block-before-first.apk", Error.BAD_BLANK_SPACE),
            new FileResult("block-at-end.apk", Error.BAD_BLANK_SPACE),
            new FileResult("block-before-eocd.apk", Error.BAD_BLANK_SPACE),
            new FileResult("block-before-cd.apk", Error.BAD_BLANK_SPACE),
            new FileResult("block-middle.apk", Error.BAD_BLANK_SPACE),
            new FileResult("v2-signed-too-large.apk", Error.BAD_V2_SIGNING_BLOCK),
            // This badly fuzzed file should return Error.FILE_COMMENT_TOO_LARGE.
            new FileResult("fcomment-too-large.apk", Error.BAD_APK),
        };
        for (FileResult test : tests) {
            RandomAccessFile file = new RandomAccessFile(testFilePath(test.filename), "r");
            FileChannel inChannel = file.getChannel();
            MappedByteBuffer buf =
                    inChannel.map(FileChannel.MapMode.READ_ONLY, 0, inChannel.size());
            buf.load();
            WebApkVerifySignature v = new WebApkVerifySignature(buf);
            try {
                @WebApkVerifySignature.Error int readError = v.read();
                if (readError == WebApkVerifySignature.Error.OK) {
                    assertEquals(test.filename, test.want, v.verifySignature(pub));
                } else {
                    assertEquals(test.filename, test.want, readError);
                }
            } catch (Exception e) {
                Assert.fail("verify exception: " + e);
            }
            buf.clear();
            inChannel.close();
            file.close();
        }
    }

    // Get the full test file path.
    private static String testFilePath(String fileName) {
        return TestDir.getTestFilePath(TEST_DATA_DIR + fileName);
    }
}
