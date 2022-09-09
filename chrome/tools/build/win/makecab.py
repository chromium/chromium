#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""usage: makecab.py [options] source [destination]
Makes cab archives of single files, using zip compression.
Acts like Microsoft makecab.exe would act if passed `/D CompressionType=MSZIP`.
If [destination] is omitted, uses source with last character replaced with _.

options:
-h, --help: print this message
/D arg: silently ignored (for compat with makecab.exe)
/L outdir: put output file in outdir
/Vn: silently ignored (for compat with makecab.exe)
"""

# A cross-platform reimplementation of the bits of makecab.exe that we use.
# cab also supports LZX compression, which has a bitstream that allows for
# a higher compression rate than zip compression (aka deflate).  But the cab
# shipped to users is built on the signing server using regular Microsoft
# makecab.exe, so having something in-tree that is good enough is good enough.

from __future__ import print_function
from collections import namedtuple
import datetime
import os
import struct
import sys
import zlib


class FlagParseError(Exception): pass


def ParseFlags(flags):
  """Parses |flags| and returns the parsed flags; returns None for --help."""
  # Can't use optparse / argparse because of /-style flags :-/
  input = None
  output = None
  output_dir = '.'
  # Parse.
  i = 0
  while i < len(flags):
    flag = flags[i]
    if flag == '-h' or flag == '--help':
      return None
    if flag.startswith('/V'):
      i += 1  # Ignore /V1 and friends.
    elif flag in ['/D', '/L']:
      if i == len(flags) - 1:
        raise FlagParseError('argument needed after ' + flag)
      if flag == '/L':
        output_dir = flags[i + 1]
      # Ignore all /D flags silently.
      i += 2
    elif (flag.startswith('-') or
          (flag.startswith('/') and not os.path.exists(flag))):
      raise FlagParseError('unknown flag ' + flag)
    else:
      if not input:
        input = flag
      elif not output:
        output = flag
      else:
        raise FlagParseError('too many paths: %s %s %s' % (input, output, flag))
      i += 1
  # Validate and set default values.
  if not input:
    raise FlagParseError('no input file')
  if not output:
    output = os.path.basename(input)[:-1] + '_'
  Flags = namedtuple('Flags', ['input', 'output', 'output_dir'])
  return Flags(input=input, output=output, output_dir=output_dir)


def WriteCab(output_file, input_file, cab_stored_filename, input_size,
             input_mtimestamp):
  """Reads data from input_file and stores its MSZIP-compressed data
  in output_file.  cab_stored_filename is the filename stored in the
  cab file, input_size is the size of the input file, and input_mtimestamp
  the mtime timestamp of the input file (must be at least midnight 1980-1-1)."""
  # Need to write (all in little-endian)::
  # 36 bytes CFHEADER cab header
  # 8 bytes CFFOLDER (a set of files compressed with the same parameters)
  # 16 bytes + filename (+ 1 byte trailing \0 for filename) CFFFILE
  # Many 8 bytes CFDATA blocks, representing 32kB chunks of uncompressed data,
  # each followed by the compressed data.
  cffile_offset = 36 + 8
  cfdata_offset = cffile_offset + 16 + len(cab_stored_filename) + 1

  chunk_size = 1 << 15
  num_chunks = int((input_size + chunk_size - 1) / chunk_size)

  # https://msdn.microsoft.com/en-us/library/bb417343.aspx#cabinet_format
  # Write CFHEADER.
  CFHEADER = ('<'
    '4s' # signature, 'MSCF'
    'I'  # reserved1, set to 0
    'I'  # cbCabinet, size of file in bytes. Not yet known, filled in later.
    'I'  # reserved2, set to 0

    'I'  # coffFiles, offset of first (and here, only) CFFILE.
    'I'  # reserved3, set to 0
    'B'  # versionMinor, currently 3. Yes, minor version is first.
    'B'  # versionMajor, currently 1.
    'H'  # cFolders, number of CFFOLDER entries.
    'H'  # cFiles, number of CFFILE entries.
    'H'  # flags, for multi-file cabinets. 0 here.

    'H'  # setID, for multi-file cabinets. 0 here.
    'H'  # iCabinet, index in multi-file cabinets. 0 here.
  )
  output_file.write(struct.pack(CFHEADER,
      b'MSCF', 0, 0, 0,
      cffile_offset, 0, 3, 1, 1, 1, 0,
      0, 0))

  # Write single CFFOLDER.
  CFFOLDER = ('<'
    'I'  # coffCabStart, offset of first CFDATA block in this folder.
    'H'  # cCFData, number of CFDATA blocks in this folder.
    'H'  # typeCompress, compression type. 1 means MS-ZIP.
  )
  output_file.write(struct.pack(CFFOLDER, cfdata_offset, num_chunks, 1))

  # Write single CFFILE.
  CFFILE = ('<'
    'I'  # cbFile, uncompressed size of this file in bytes.
    'I'  # uoffFolderStart, uncompressed offset of this file in folder.
    'H'  # iFolder, index into CFFOLDER area.
    'H'  # date, in the format ((year-1980) << 9) + (month << 5) + (day),
         # where month={1..12} and day={1..31}.
    'H'  # time, in the format (hour << 11)+(minute << 5)+(seconds/2),
         # where hour={0..23}.
    'H'  # attribs, 1: read-only
                  # 2: hidden
                  # 4: system file
                  # 0x20: file modified since last backup
                  # 0x40: run after extraction
                  # 0x80: name contains UTF
  )  # Followed by szFile, the file's name.
  assert output_file.tell() == cffile_offset
  mtime = datetime.datetime.fromtimestamp(input_mtimestamp)
  date = (mtime.year - 1980) << 9 | mtime.month << 5 | mtime.day
  # TODO(thakis): hour seems to be off by 1 from makecab.exe (DST?)
  time = mtime.hour << 11 | mtime.minute << 5 | int(mtime.second / 2)
  output_file.write(struct.pack(CFFILE, input_size, 0, 0, date, time, 0))
  output_file.write(cab_stored_filename.encode() + b'\0')

  # Write num_chunks many CFDATA headers, followed by the compressed data.
  assert output_file.tell() == cfdata_offset
  CFDATA = ('<'
    'I'  # checksum. Optional and expensive to compute in Python, so write 0.
    'H'  # cbData, number of compressed bytes in this block.
    'H'  # cbUncomp, size after decompressing. 1 << 15 for all but the last.
  )
  # Read input data in chunks of 32kB, compress and write out compressed data.
  for _ in range(num_chunks):
    chunk = input_file.read(chunk_size)
    # Have to use compressobj instead of compress() so we can pass a negative
    # window size to remove header and trailing checksum.
    # Compression level 6 runs about 8x as fast as makecab.exe's LZX compression
    # while producing a 45% larger file.  (Interestingly, it also runs
    # about 5x as fast as makecab.exe's MSZIP compression while being about
    # 4.8% larger -- so it might be possible to write an LZX compressor that's
    # much faster without being so much larger.)  Compression level 9 isn't
    # very different.  Level 1 is another ~30% faster and 10% larger.
    # Since 6 is ok and the default, let's go with that.
    # Remember: User-shipped bits get recompressed on the signing server.
    zlib_obj = zlib.compressobj(
        zlib.Z_DEFAULT_COMPRESSION, zlib.DEFLATED, -zlib.MAX_WBITS)
    compressed = zlib_obj.compress(chunk) + zlib_obj.flush()
    compressed_size = 2 + len(compressed)  # Also count 0x43 0x4b magic header.
    # cab spec: "Each data block represents 32k uncompressed, except that the
    # last block in a folder may be smaller. A two-byte MSZIP signature precedes
    # the compressed encoding in each block, consisting of the bytes 0x43, 0x4B.
    # The maximum compressed size of each MSZIP block is 32k + 12 bytes."
    assert compressed_size <= chunk_size + 12
    output_file.write(struct.pack(CFDATA, 0, compressed_size, len(chunk)))
    output_file.write(b'\x43\x4b')  # MSZIP magic block header.
    output_file.write(compressed)
  outfile_size = output_file.tell()

  # Now go back and fill in missing size in CFHEADER.
  output_file.seek(8)  # cbCabinet, size of file in bytes.
  output_file.write(struct.pack('<I', outfile_size))


def main():
  try:
    flags = ParseFlags(sys.argv[1:])
  except FlagParseError as arg_error:
    print('makecab.py: error:', arg_error.message, file=sys.stderr)
    print('pass --help for usage', file=sys.stderr)
    sys.exit(1)
  if not flags:  # --help got passed
    print(__doc__)
    sys.exit(0)
  if not os.path.exists(flags.input):
    print('makecab.py: error: input file %s does not exist' % flags.input,
          file=sys.stderr)
    sys.exit(1)
  with open(os.path.join(flags.output_dir, flags.output), 'wb') as output_file:
    cab_stored_filename = os.path.basename(flags.input)
    input_mtimestamp = os.path.getmtime(flags.input)
    input_size = os.path.getsize(flags.input)
    with open(flags.input, 'rb') as input_file:
      WriteCab(output_file, input_file, cab_stored_filename, input_size,
               input_mtimestamp)


if __name__ == '__main__':
  main()
