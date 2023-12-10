// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapk.lib.client;

import static java.nio.ByteOrder.LITTLE_ENDIAN;

import androidx.annotation.IntDef;

import org.chromium.base.Log;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.security.PublicKey;
import java.security.Signature;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * WebApkVerifySignature reads in the APK file and verifies the WebApk signature. It reads the
 * signature from the zip comment and verifies that it was signed by the public key passed.
 */
public class WebApkVerifySignature {
    /** Errors codes. */
    @IntDef({
        Error.OK,
        Error.BAD_APK,
        Error.EXTRA_FIELD_TOO_LARGE,
        Error.FILE_COMMENT_TOO_LARGE,
        Error.INCORRECT_SIGNATURE,
        Error.SIGNATURE_NOT_FOUND,
        Error.TOO_MANY_META_INF_FILES,
        Error.BAD_BLANK_SPACE,
        Error.BAD_V2_SIGNING_BLOCK
    })
    @SuppressWarnings("JavaLangClash")
    @Retention(RetentionPolicy.SOURCE)
    public @interface Error {
        int OK = 0;
        int BAD_APK = 1;
        int EXTRA_FIELD_TOO_LARGE = 2;
        int FILE_COMMENT_TOO_LARGE = 3;
        int INCORRECT_SIGNATURE = 4;
        int SIGNATURE_NOT_FOUND = 5;
        int TOO_MANY_META_INF_FILES = 6;
        int BAD_BLANK_SPACE = 7;
        int BAD_V2_SIGNING_BLOCK = 8;
    }

    private static final String TAG = "WebApkVerifySignature";

    /** End Of Central Directory Signature. */
    private static final long EOCD_SIG = 0x06054b50;

    /** Central Directory Signature. */
    private static final long CD_SIG = 0x02014b50;

    /** Local File Header Signature. */
    private static final long LFH_SIG = 0x04034b50;

    /** Data descriptor Signature. */
    private static final long DATA_DESCRIPTOR_SIG = 0x08074b50;

    /** Minimum end-of-central-directory size in bytes, including variable length file comment. */
    private static final int MIN_EOCD_SIZE = 22;

    /** Max end-of-central-directory size in bytes permitted. */
    private static final int MAX_EOCD_SIZE = 64 * 1024;

    /** Maximum number of META-INF/ files (allowing for dual signing). */
    private static final int MAX_META_INF_FILES = 5;

    /** The signature algorithm used (must also match with HASH). */
    private static final String SIGNING_ALGORITHM = "SHA256withECDSA";

    /** Maximum expected V2 signing block size with 3 signatures */
    private static final int MAX_V2_SIGNING_BLOCK_SIZE = 8192 * 3;

    /** The magic string for v2 signing. */
    private static final String V2_SIGNING_MAGIC = "APK Sig Block 42";

    /**
     * The pattern we look for in the APK/zip comment for signing key. An example is
     * "webapk:0000:<hexvalues>". This pattern can appear anywhere in the comment but must be
     * separated from any other parts with a separator that doesn't look like a hex character.
     */
    private static final Pattern WEBAPK_COMMENT_PATTERN =
            Pattern.compile("webapk:\\d+:([a-fA-F0-9]+)");

    /** Maximum file comment length permitted. */
    private static final int MAX_FILE_COMMENT_LENGTH = 0;

    /** The memory buffer we are going to read the zip from. */
    private final ByteBuffer mBuffer;

    /** Number of total central directory (zip entry) records. */
    private int mRecordCount;

    /** Byte offset from the start where the central directory is found. */
    private int mCentralDirOffset;

    /** Byte offset from the start where the EOCD is found. */
    private int mEndOfCentralDirOffset;

    /** The zip archive comment as a UTF-8 string. */
    private String mComment;

    /**
     * Sorted list of 'blocks' of memory we will cryptographically hash. We sort the blocks by
     * filename to ensure a repeatable order.
     */
    private ArrayList<Block> mBlocks;

    /** Block contains metadata about a zip entry. */
    private static class Block implements Comparable<Block> {
        String mFilename;
        int mPosition;
        int mHeaderSize;
        int mCompressedSize;

        Block(String filename, int position, int compressedSize) {
            mFilename = filename;
            mPosition = position;
            mHeaderSize = 0;
            mCompressedSize = compressedSize;
        }

        /** Added for Comparable, sort lexicographically. */
        @Override
        public int compareTo(Block o) {
            return mFilename.compareTo(o.mFilename);
        }

        /** Comparator for sorting the list by position ascending. */
        public static Comparator<Block> positionComparator =
                new Comparator<Block>() {
                    @Override
                    public int compare(Block b1, Block b2) {
                        return b1.mPosition - b2.mPosition;
                    }
                };

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof Block)) return false;
            return mFilename.equals(((Block) o).mFilename);
        }

        @Override
        public int hashCode() {
            return mFilename.hashCode();
        }
    }

    /** Constructor simply 'connects' to buffer passed. */
    public WebApkVerifySignature(ByteBuffer buffer) {
        mBuffer = buffer;
        mBuffer.order(LITTLE_ENDIAN);
    }

    /**
     * Read in the comment and directory. If there is no parseable comment we won't read the
     * directory as there is no point (for speed). On success, all of our private variables will be
     * set.
     *
     * @return OK on success.
     */
    public @Error int read() {
        try {
            @Error int err = readEOCD();
            if (err != Error.OK) {
                return err;
            }
            // Short circuit if no comment found.
            if (parseCommentSignature(mComment) == null) {
                return Error.SIGNATURE_NOT_FOUND;
            }
            err = readDirectory();
            if (err != Error.OK) {
                return err;
            }
        } catch (Exception e) {
            return Error.BAD_APK;
        }
        return Error.OK;
    }

    /**
     * verifySignature hashes all the files and then verifies the signature.
     *
     * @param pub The public key that it should be verified against.
     * @return Error.OK if the public key signature verifies.
     */
    public @Error int verifySignature(PublicKey pub) {
        byte[] sig = parseCommentSignature(mComment);
        if (sig == null || sig.length == 0) {
            return Error.SIGNATURE_NOT_FOUND;
        }
        try {
            Signature signature = Signature.getInstance(SIGNING_ALGORITHM);
            signature.initVerify(pub);
            int err = calculateHash(signature);
            if (err != Error.OK) {
                return err;
            }
            return signature.verify(sig) ? Error.OK : Error.INCORRECT_SIGNATURE;
        } catch (Exception e) {
            Log.e(TAG, "Exception calculating signature", e);
            return Error.INCORRECT_SIGNATURE;
        }
    }

    /**
     * calculateHash goes through each file listed in blocks and calculates the SHA-256
     * cryptographic hash.
     *
     * @param sig Signature object you can call update on.
     */
    public @Error int calculateHash(Signature sig) throws Exception {
        Collections.sort(mBlocks);
        int metaInfCount = 0;
        for (Block block : mBlocks) {
            if (block.mFilename.indexOf("META-INF/") == 0) {
                metaInfCount++;
                if (metaInfCount > MAX_META_INF_FILES) {
                    // TODO(scottkirkwood): Add whitelist of files.
                    return Error.TOO_MANY_META_INF_FILES;
                }

                // Files that begin with META-INF/ are not part of the hash.
                // This is because these signatures are added after we comment signed the rest of
                // the APK.
                continue;
            }

            // Hash the filename length and filename to prevent Horton principle violation.
            byte[] filename = block.mFilename.getBytes();
            sig.update(intToLittleEndian(filename.length));
            sig.update(filename);

            // Also hash the block length for the same reason.
            sig.update(intToLittleEndian(block.mCompressedSize));

            seek(block.mPosition + block.mHeaderSize);
            ByteBuffer slice = mBuffer.slice();
            slice.limit(block.mCompressedSize);
            sig.update(slice);
        }
        return Error.OK;
    }

    /**
     * intToLittleEndian converts an integer to a little endian array of bytes.
     *
     * @param value Integer value to convert.
     * @return Array of bytes.
     */
    private byte[] intToLittleEndian(int value) {
        ByteBuffer buffer = ByteBuffer.allocate(4);
        buffer.order(LITTLE_ENDIAN);
        buffer.putInt(value);
        return buffer.array();
    }

    /**
     * Extract the bytes of the signature from the comment. We expect "webapk:0000:<hexvalues>"
     * comment followed by hex values. Currently we ignore the "key id" which is always "0000".
     *
     * @return the bytes of the signature.
     */
    static byte[] parseCommentSignature(String comment) {
        Matcher m = WEBAPK_COMMENT_PATTERN.matcher(comment);
        if (!m.find()) {
            return null;
        }
        String s = m.group(1);
        return hexToBytes(s);
    }

    /**
     * Reads the End of Central Directory Record.
     *
     * @return Error.OK on success.
     */
    private @Error int readEOCD() {
        int start = findEOCDStart();
        if (start < 0) {
            return Error.BAD_APK;
        }
        mEndOfCentralDirOffset = start;

        //  Signature(4), Disk Number(2), Start disk number(2), Records on this disk (2)
        seek(start + 10);
        mRecordCount = read2(); // Number of Central Directory records
        seekDelta(4); // Size of central directory
        mCentralDirOffset = read4(); // as bytes from start of file.
        int commentLength = read2();
        mComment = readString(commentLength);
        if (mBuffer.position() < mBuffer.limit()) {
            // We should have read every byte to the end of the file by this time.
            return Error.BAD_BLANK_SPACE;
        }
        return Error.OK;
    }

    /**
     * Reads the central directory and populates {@link mBlocks} with data about each entry.
     *
     * @return Error.OK on success.
     */
    @Error
    int readDirectory() {
        mBlocks = new ArrayList<>(mRecordCount);
        seek(mCentralDirOffset);
        for (int i = 0; i < mRecordCount; i++) {
            int signature = read4();
            if (signature != CD_SIG) {
                Log.d(TAG, "Missing Central Directory Signature");
                return Error.BAD_APK;
            }
            // CreatorVersion(2), ReaderVersion(2), Flags(2), CompressionMethod(2)
            // ModifiedTime(2), ModifiedDate(2), CRC32(4) = 16 bytes
            seekDelta(16);
            int compressedSize = read4();
            seekDelta(4); // uncompressed size
            int fileNameLength = read2();
            int extraLen = read2();
            int fileCommentLength = read2();
            seekDelta(8); // DiskNumberStart(2), Internal Attrs(2), External Attrs(4)
            int offset = read4();
            String filename = readString(fileNameLength);
            seekDelta(extraLen + fileCommentLength);
            if (fileCommentLength > MAX_FILE_COMMENT_LENGTH) {
                return Error.FILE_COMMENT_TOO_LARGE;
            }
            mBlocks.add(new Block(filename, offset, compressedSize));
        }

        if (mBuffer.position() != mEndOfCentralDirOffset) {
            // At this point we should be exactly at the EOCD start.
            return Error.BAD_BLANK_SPACE;
        }

        // We need blocks to be sorted by position at this point.
        Collections.sort(mBlocks, Block.positionComparator);
        int lastByte = 0;

        // Read the 'local file header' block to the size of the header in bytes.
        for (Block block : mBlocks) {
            if (block.mPosition != lastByte) {
                return Error.BAD_BLANK_SPACE;
            }

            seek(block.mPosition);
            int signature = read4();
            if (signature != LFH_SIG) {
                Log.d(TAG, "LFH Signature missing");
                return Error.BAD_APK;
            }
            // ReaderVersion(2)
            seekDelta(2);
            int flags = read2();
            // CompressionMethod(2), ModifiedTime (2), ModifiedDate(2), CRC32(4), CompressedSize(4),
            // UncompressedSize(4) = 18 bytes
            seekDelta(18);
            int fileNameLength = read2();
            int extraFieldLength = read2();

            block.mHeaderSize =
                    (mBuffer.position() - block.mPosition) + fileNameLength + extraFieldLength;

            lastByte = block.mPosition + block.mHeaderSize + block.mCompressedSize;
            if ((flags & 0x8) != 0) {
                seek(lastByte);
                if (read4() == DATA_DESCRIPTOR_SIG) {
                    // Data descriptor, style 1: sig(4), crc-32(4), compressed size(4),
                    // uncompressed size(4) = 16 bytes
                    lastByte += 16;
                } else {
                    // Data descriptor, style 2: crc-32(4), compressed size(4),
                    // uncompressed size(4) = 12 bytes
                    lastByte += 12;
                }
            }
        }
        if (lastByte != mCentralDirOffset) {
            seek(mCentralDirOffset - V2_SIGNING_MAGIC.length());
            String magic = readString(V2_SIGNING_MAGIC.length());
            if (V2_SIGNING_MAGIC.equals(magic)) {
                // Only if we have a v2 signature do we allow medium sized gap between the last
                // block and the start of the central directory.
                if (mCentralDirOffset - lastByte > MAX_V2_SIGNING_BLOCK_SIZE) {
                    return Error.BAD_V2_SIGNING_BLOCK;
                }
            } else {
                return Error.BAD_BLANK_SPACE;
            }
        }
        return Error.OK;
    }

    /**
     * We search buffer for EOCD_SIG and return the location where we found it. If the file has no
     * comment it should seek only once.
     *
     * @return Offset from start of buffer or -1 if not found.
     */
    private int findEOCDStart() {
        // TODO(scottkirkwood): Use a Boyer-Moore search algorithm.
        int offset = mBuffer.limit() - MIN_EOCD_SIZE;
        int minSearchOffset = Math.max(0, offset - MAX_EOCD_SIZE);
        for (; offset >= minSearchOffset; offset--) {
            seek(offset);
            if (read4() == EOCD_SIG) {
                // found!
                return offset;
            }
        }
        return -1;
    }

    /**
     * Seek to this position.
     *
     * @param offset offset from start of file.
     */
    private void seek(int offset) {
        mBuffer.position(offset);
    }

    /**
     * Skip forward this number of bytes.
     *
     * @param delta number of bytes to seek forward.
     */
    private void seekDelta(int delta) {
        mBuffer.position(mBuffer.position() + delta);
    }

    /**
     * Reads two bytes in little endian format.
     *
     * @return short value read (as an int).
     */
    private int read2() {
        return mBuffer.getShort();
    }

    /**
     * Reads four bytes in little endian format.
     *
     * @return value read.
     */
    private int read4() {
        return mBuffer.getInt();
    }

    /** Read {@link length} many bytes into a string. */
    private String readString(int length) {
        if (length <= 0) {
            return "";
        }
        byte[] bytes = new byte[length];
        mBuffer.get(bytes);
        return new String(bytes);
    }

    /**
     * Convert a hex string into bytes. We store hex in the signature as zip tools often don't like
     * binary strings.
     */
    static byte[] hexToBytes(String s) {
        int len = s.length();
        if (len % 2 != 0) {
            // Odd number of nibbles.
            return null;
        }
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] =
                    (byte)
                            ((Character.digit(s.charAt(i), 16) << 4)
                                    + Character.digit(s.charAt(i + 1), 16));
        }
        return data;
    }
}
