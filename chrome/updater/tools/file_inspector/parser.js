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
        console.log(node);
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
              span.title =
                  "offset 0x" + i.toString(16) + " (" + i + ") = " + decimal;
            }
            ospan.appendChild(span);
          }
          ospan.setAttribute("class", "byte_" + node.start);
          ospan.addEventListener(
              "mouseover", () => hover(node.start, true, hoverSet));
          ospan.addEventListener(
              "mouseout", () => hover(node.start, false, hoverSet));
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
    for (let t of ["NoOp", "Px86", "Px64", "Ex86", "Ex64",
                   "EA32", "EA64", "DEX ", "ZTF "]) {
      if (val == (
          (t.charCodeAt(3) << 24) |
          (t.charCodeAt(2) << 16) |
          (t.charCodeAt(1) << 8) |
          t.charCodeAt(0))) {
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
    console.log("Buffer at " + (offset - 4) + " has content size " + size);
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
  console.log("Beginning element parsing at " + offset + ".");
  // elements
  for (let i = 0; i < structure.elements_count.value; i++) {
    console.log("Parsing element " + i + " at " + offset + ".");
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
    console.log("Beginning parsing of extra targets at " + offset + ".");
    for (let i = 0; i < element.pool_count.value; i++) {
      element.extra_targets.push({
        "pool_tag": uint8(),
        "extra_targets": buffer(),
      });
    }
  }

  return structure;
}

const parserMap = {
  "zucc": parseZucchini,
};
