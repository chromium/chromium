// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// render a hex dump of the file, plus structural insights about its contents
export function render(data, type, hexdump, json, insights) {
  let hoverSet = {};
  if (parserMap.hasOwnProperty(type)) {
    try {
      let structure = parserMap[type](data);
      json.appendChild(
          document.createTextNode(JSON.stringify(structure, null, " ")));
      let insightEle = document.createElement("insight-box");
      insightEle.title = "FILE";
      insightEle.data = structure;
      insightEle.hoverSet = hoverSet;
      insights.appendChild(insightEle);

      // Print hex dump of non-opaque sections.
      function printHexDump(node) {
        if (node instanceof ByteSpan) {
          let ospan = document.createElement("span");
          for (let i = node.start; i < node.end; i++) {
            let span = document.createElement("span");
            if (i > node.start + 8 && i < node.end - 8) {
              span.appendChild(document.createTextNode(".."));
              span.title = "skipping bytes";
              i = node.end - 8;
            } else {
              let decimal = data.getUint8(i);
              let hexadecimal = decimal.toString(16);
              if (hexadecimal.length == 1) hexadecimal = "0" + hexadecimal;
              span.appendChild(document.createTextNode(hexadecimal));
              span.title = `offset 0x ${i.toString(16)} (${i}) = ${decimal}`;
            }
            ospan.appendChild(span);
          }
          ospan.setAttribute("class", "byte_" + node.start);
          ospan.addEventListener("mouseover",
              () => hover(node.start, true, hoverSet));
          ospan.addEventListener("mouseout",
              () => hover(node.start, false, hoverSet));
          hexdump.appendChild(ospan);
        } else {
          for (let i in node) printHexDump(node[i]);
        }
      }
      printHexDump(structure);
    } catch (error) {
      console.log(error);
      insights.appendChild(document.createTextNode(error.toString()));
    }
  } else {
    insights.appendChild(document.createTextNode("Unknown file type."));
  }
}

export function hover(index, state, hoverSet) {
  for (let e of document.querySelectorAll(".byte_" + index)) {
    e.style.backgroundColor = state ? "#ccccff" : "";
  }
  if (hoverSet.hasOwnProperty(index)) {
    hoverSet[index].style.backgroundColor = state ? "#ccccff" : "";
  }
}

// Each ByteSpan is a number of bytes read from a file.
export class ByteSpan {
  /* Class properties:
       value: a singleton value to associate with this finding
       start: byte index. If -1, no associated bytes
       end: byte index. If -1, no associated bytes
  */

  constructor(value, start, end) {
    this.value = value;
    this.start = start;
    this.end = end;
  }

  addChild(c) {
    this.value.push(c);
  }

  toJSON() {
    return this.value;
  }

  toString() {
    return this.value.toString();
  }
}

// https://chromium.googlesource.com/chromium/src/+/main/components/zucchini/README.md
function parseZucchini(data) {
  let offset = 0;

  function uint32(littleEndian = true, transform = x => x) {
    return new ByteSpan(
      transform(data.getUint32(offset, littleEndian)),
      offset,
      offset += 4);
  }

  function uint16(littleEndian = true) {
    return new ByteSpan(
      data.getUint16(offset, littleEndian),
      offset,
      offset += 2);
  }

  function uint8() {
    return new ByteSpan(
      data.getUint8(offset),
      offset,
      offset += 1);
  }

  function skip(amount) {
    return new ByteSpan("[" + amount + " octets]", offset, offset += amount);
  }

  function exeType(val) {
    for (let t of [
        "NoOp", "Px86", "Px64", "Ex86", "Ex64", "EA32", "EA64", "DEX ",
        "ZTF "]) {
      if (val == (
          (t.charCodeAt(3) << 24)
          | (t.charCodeAt(2) << 16)
          | (t.charCodeAt(1) << 8)
          | t.charCodeAt(0))) {
        return t + " (" + val + ")";
      }
    }
    return "???? (" + val + ")";
  }

  function magic(val) {
    return "0x" + val.toString(16);
  }

  function buffer() {
    let size = uint32();
    return {
      "size": size,
      "content": skip(size.value),
    };
  }

  let structure = {
    "header": {
      "magic": uint32(false, magic),
      "major_version": uint16(),
      "minor_version": uint16(),
      "old_size": uint32(),
      "old_crc": uint32(),
      "new_size": uint32(),
      "new_crc": uint32(),
    },
    "elements_count": uint32(),
    "elements": [],
  };
  // elements
  for (let i = 0; i < structure.elements_count.value; i++) {
    let element = {
      "header": {
        "old_offset": uint32(),
        "old_length": uint32(),
        "new_offset": uint32(),
        "new_length": uint32(),
        "exe_type": uint32(true, exeType),
        "version": uint16(),
      },
      "equivalences": {
        "src_skip": buffer(),
        "dst_skip": buffer(),
        "copy_count": buffer(),
      },
      "extra_data": {
        "extra_data": buffer(),
      },
      "raw_deltas": {
        "raw_delta_skip": buffer(),
        "raw_delta_diff": buffer(),
      },
      "reference_deltas": {
        "reference_delta": buffer(),
      },
      "pool_count": uint32(),
      "extra_targets": [],
    };
    structure.elements.push(element);
    for (let i = 0; i < element.pool_count.value; i++) {
      element.extra_targets.push({
        "pool_tag": uint8(),
        "extra_targets": buffer(),
      });
    }
  }

  return structure;
}

// https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
function parseZip(data) {
  let offset = 0;

  function uint64(littleEndian = true) {
    return new ByteSpan(
      data.getUint64(offset, littleEndian),
      offset,
      offset += 8);
  }

  function uint32(littleEndian = true, transform = x => x) {
    return new ByteSpan(
      transform(data.getUint32(offset, littleEndian)),
      offset,
      offset += 4);
  }

  function int32(littleEndian = true, transform = x => x) {
    return new ByteSpan(
      transform(data.getInt32(offset, littleEndian)),
      offset,
      offset += 4);
  }

  function uint16(littleEndian = true, transform = x => x) {
    return new ByteSpan(
      transform(data.getUint16(offset, littleEndian)),
      offset,
      offset += 2);
  }

  function msdos_date() {
    return uint16(true, v => {
      let day = v & 0x1F;
      let month = (v >> 5) & 0xF;
      let year = ((v >> 9) & 0x7F) + 1980;
      return `${year}-${month}-${day}`;
    });
  }

  function msdos_time() {
    return uint16(true, v => {
      let second = (v & 0x1F) * 2;
      let minute = (v >> 5) & 0x3F;
      let hour = (v >> 11) & 0x1F;
      return `${hour}:${minute}:${second}`;
    });
  }

  function uint8() {
    return new ByteSpan(
      data.getUint8(offset),
      offset,
      offset += 1);
  }

  function skip(amount) {
    return new ByteSpan("[" + amount + " octets]", offset, offset += amount);
  }

  function utf8(amount) {
    const s =
        new TextDecoder().decode(data.buffer.slice(offset, offset + amount));
    return new ByteSpan(
        `[${amount} octets] (UTF8): ${s}`, offset, offset += amount);
  }

  function magic(val) {
    return "0x" + val.toString(16);
  }

  function extra_field() {
    let header = uint16();
    let size = uint16();
    if (header.value == 0x5455) {
      if (size == 5) {
        return {
          "header_id": new ByteSpan(
              "Extended timestamp (\"UT\")", header.start, header.end),
          "size": size,
          "flags": uint8(),
          "modified_time": int32(true, v => new Date(v * 1000).toISOString()),
        }
      } else if (size == 13) {
        return {
          "header_id": new ByteSpan(
              "Extended timestamp (\"UT\")", header.start, header.end),
          "size": size,
          "flags": uint8(),
          "modified_time": int32(true, v => new Date(v * 1000).toISOString()),
          "access_time": int32(true, v => new Date(v * 1000).toISOString()),
          "creation_time": int32(true, v => new Date(v * 1000).toISOString()),
        }
      }
    }
    return {
      "header_id": new ByteSpan(magic(header.value), header.start, header.end),
      "size": size,
      "content": skip(size.value),
    };
  }

  function buffer16() {
    let size = uint16();
    return {
      "size": size,
      "content": skip(size.value),
    };
  }

  let structure = {
    "prefix": {},
    "files": [],
    "archive_decryption_header": {},
    "archive_extra_data_record": {},
    "central_directory": {
      "headers": [],
      "signature": {},
    },
    "zip64_eocd_record": {},
    "zip64_eocd_locator": {},
    "eocd_record": {},
  };

  // Some ZIP files have data pre-pended. Try to find the first local file
  // header.
  let prefix_length = (() => {
    offset = 0;
    while (offset < data.byteLength - 4) {
      let v = data.getUint32(offset, true);
      if (v == 0x04034b50 || v == 0x08064b50 || v == 0x02014b50) return offset;
      offset++;
    }
  })();
  offset = 0;
  structure.prefix = skip(prefix_length);


  // Find EOCD. If the EOCD magic number is in the comment, this will find the
  // wrong one.
  let offset_eocdr = (() => {
    offset = data.byteLength - 10;
    while (offset > 0) {
      for (let i = 0; i + offset < data.byteLength - 4 && i < 14; i++) {
        if (data.getUint32(offset + i, true) == 0x06054b50) {
          return offset + i;
        }
      }
      offset -= 10;
    }
    throw "No EOCD record found.";
  })();
  offset = offset_eocdr;
  structure.eocd_record = {
    "eocd_signature": uint32(true, magic),
    "disk_number": uint16(),
    "disk_with_start_of_eocd": uint16(),
    "total_entries_on_disk": uint16(),
    "total_entries": uint16(),
    "size_of_cd": uint32(),
    "cd_start_offset": uint32(),
    "zip_comment": buffer16(),
  };

  let offset_z64_eocdl = offset_eocdr - 4 - 8 - 4 - 4;
  if (data.getUint32(offset_z64_eocdl, true) == 0x07064b50) {
    offset = offset_z64_eocdl;
    structure.zip64_eocd_locator = {
      "z64_eocdl_signature": uint32(true, magic),
      "z64_eocd_disk": uint32(),
      "z64_eocdr_offset": uint64(),
      "total_disks": uint32(),
    };

    offset = structure.zip64_eocd_locator.z64_eocdr_offset + prefix_length;
    structure.zip64_eocd_record = {
      "zip64_eocdr_signature": uint32(true, magic),
      "zip64_eocdr_size": uint64(),
      "version_made_by": uint16(),
      "version_needed_to_extract": uint16(),
      "disk_number": uint32(),
      "disk_with_start_of_eocd": uint32(),
      "total_entries_on_disk": uint64(),
      "total_entries": uint64(),
      "size_of_cd": uint64(),
      "cd_start_offset": uint64(),
      "extensible_data": {},
    }
    let remainingSize = (
        structure.zip64_eocd_record.zip64_eocdr_size.value - 8 - 8 - 8 - 8
        - 4 - 4 - 2 - 2);
    if (remainingSize > 0) {
      structure.zip64_eocd_record.extensible_data = skip(remainingSize);
    }
  }

  // Note: this implementation ignores multiple disks.

  let offset_cd = (
      structure.zip64_eocd_record.cd_start_offset
      ?? structure.eocd_record.cd_start_offset).value + prefix_length;
  let num_entries = (
      structure.zip64_eocd_record.total_entries
      ?? structure.eocd_record.total_entries).value;
  offset = offset_cd;
  for (let i = 0; i < num_entries; i++) {
    let h = {
      "central_file_header_signature": uint32(true, magic),
      "version_made_by": uint16(),
      "version_needed_to_extract": uint16(),
      "general_purpose_bit_flag": uint16(true, magic),
      "compression_method": uint16(),
      "last_mod_file_time": msdos_time(),
      "last_mod_file_date": msdos_date(),
      "crc32": uint32(),
      "compressed_size": uint32(),
      "uncompressed_size": uint32(),
      "file_name_length": uint16(),
      "extra_field_length": uint16(),
      "file_comment_length": uint16(),
      "disk_number_start": uint16(),
      "internal_file_attributes": uint16(),
      "external_file_attributes": uint32(),
      "relative_offset_of_local_header": uint32(),
      "file_name": {},
      "extra_field": [],
      "file_comment": {},
    }
    h.file_name = utf8(h.file_name_length.value);
    let extra_field_end = offset + h.extra_field_length.value;
    while (offset < extra_field_end) {
      h.extra_field.push(extra_field());
    }
    h.file_comment = utf8(h.file_comment_length.value);
    structure.central_directory.headers.push(h);
  }

  for (let e of structure.central_directory.headers) {
    offset = e.relative_offset_of_local_header.value + prefix_length;
    let h = {
      "local_file_header_signature": uint32(true, magic),
      "version_needed_to_extract": uint16(),
      "general_purpose_bit_flag": uint16(true, magic),
      "compression_method": uint16(),
      "last_mod_file_time": msdos_time(),
      "last_mod_file_date": msdos_date(),
      "crc32": uint32(),
      "compressed_size": uint32(),
      "uncompressed_size": uint32(),
      "file_name_length": uint16(),
      "extra_field_length": uint16(),
      "file_name": {},
      "extra_field": [],
      "content": {},
    };
    h.file_name = utf8(h.file_name_length.value);
    let extra_field_end = offset + h.extra_field_length.value;
    while (offset < extra_field_end) {
      h.extra_field.push(extra_field());
    }
    h.content = skip(h.compressed_size.value);
    structure.files.push(h);
  }

  return structure;
}

const parserMap = {
  "zucc": parseZucchini,
  "zip": parseZip,
};
