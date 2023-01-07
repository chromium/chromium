// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

function RunMetadataTest(filename, callOptions, verifyMetadataFunction) {
  function getMediaFileSystemsCallback(results) {
    chrome.test.assertEq(1, results.length);
    var gallery = results[0];
    gallery.root.getFile(filename, {create: false}, verifyFileEntry,
      chrome.test.fail);
  }

  function verifyFileEntry(fileEntry) {
    fileEntry.file(verifyFile, chrome.test.fail)
  }

  function verifyFile(file) {
    mediaGalleries.getMetadata(file, callOptions, verifyMetadataFunction);
  }

  mediaGalleries.getMediaFileSystems(getMediaFileSystemsCallback);
}

function ImageMIMETypeOnlyTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("image/jpeg", metadata.mimeType);

    chrome.test.assertEq(0, metadata.attachedImages.length);

    chrome.test.succeed();
  }

  RunMetadataTest("test.jpg", {metadataType: 'mimeTypeOnly'}, verifyMetadata);
}

function InvalidMultimediaFileTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq(null, metadata);

    chrome.test.succeed();
  }

  // Read a file that is not audio or video.
  // We use getMetadata directly to test with invalid media data.
  chrome.mediaGalleries.getMetadata(new Blob([]), verifyMetadata);
}

function MP3MIMETypeOnlyTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("audio/mpeg", metadata.mimeType);
    chrome.test.assertEq(undefined, metadata.title);

    chrome.test.assertEq(0, metadata.attachedImages.length);

    chrome.test.succeed();
  }

  RunMetadataTest("id3_png_test.mp3", {metadataType: 'mimeTypeOnly'},
                  verifyMetadata);
}

function MP3TagsTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("audio/mpeg", metadata.mimeType);
    chrome.test.assertEq("Airbag", metadata.title);
    chrome.test.assertEq("Radiohead", metadata.artist);
    chrome.test.assertEq("OK Computer", metadata.album);
    chrome.test.assertEq(1, metadata.track);
    chrome.test.assertEq("Alternative", metadata.genre);

    chrome.test.assertEq(3, metadata.rawTags.length);

    chrome.test.assertEq("mp3", metadata.rawTags[0].type);
    chrome.test.assertEq("OK Computer", metadata.rawTags[0].tags["album"]);
    chrome.test.assertEq("Radiohead", metadata.rawTags[0].tags["artist"]);
    chrome.test.assertEq("1997", metadata.rawTags[0].tags["date"]);
    chrome.test.assertEq("Lavf54.4.100", metadata.rawTags[0].tags["encoder"]);
    chrome.test.assertEq("Alternative", metadata.rawTags[0].tags["genre"]);
    chrome.test.assertEq("Airbag", metadata.rawTags[0].tags["title"]);
    chrome.test.assertEq("1", metadata.rawTags[0].tags["track"]);

    chrome.test.assertEq("mp3", metadata.rawTags[1].type);

    chrome.test.assertEq("png", metadata.rawTags[2].type);

    chrome.test.assertEq(0, metadata.attachedImages.length);

    chrome.test.succeed();
  }

  return RunMetadataTest("id3_png_test.mp3", {metadataType: 'mimeTypeAndTags'},
                         verifyMetadata);
}

function MP3AttachedImageTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("audio/mpeg", metadata.mimeType);
    chrome.test.assertEq("Airbag", metadata.title);
    chrome.test.assertEq("Radiohead", metadata.artist);
    chrome.test.assertEq("OK Computer", metadata.album);
    chrome.test.assertEq(1, metadata.track);
    chrome.test.assertEq("Alternative", metadata.genre);

    chrome.test.assertEq(1, metadata.attachedImages.length);
    chrome.test.assertEq('image/png', metadata.attachedImages[0].type);
    chrome.test.assertEq(155752, metadata.attachedImages[0].size);

    var reader = new FileReader();
    reader.onload = function verifyBlobContents(event) {
      var first = new Uint8Array(reader.result, 0, 8);
      var last = new Uint8Array(reader.result, reader.result.byteLength - 8, 8);
      chrome.test.assertEq("\x89PNG\r\n\x1a\n",
                           String.fromCharCode.apply(null, first));
      chrome.test.assertEq("IEND\xae\x42\x60\x82",
                           String.fromCharCode.apply(null, last));

      chrome.test.succeed();
    }
    reader.readAsArrayBuffer(metadata.attachedImages[0]);
  }

  return RunMetadataTest("id3_png_test.mp3", {}, verifyMetadata);
}

function RotatedVideoTest() {
  function verifyMetadata(metadata) {
    chrome.test.assertEq("video/mp4", metadata.mimeType);
    chrome.test.assertEq(90, metadata.rotation);

    chrome.test.assertEq(3, metadata.rawTags.length);

    chrome.test.assertEq("mov,mp4,m4a,3gp,3g2,mj2", metadata.rawTags[0].type);
    chrome.test.assertEq("isom3gp4",
                         metadata.rawTags[0].tags["compatible_brands"]);
    chrome.test.assertEq("2014-02-11T00:39:25.000000Z",
                         metadata.rawTags[0].tags["creation_time"]);
    chrome.test.assertEq("isom", metadata.rawTags[0].tags["major_brand"]);
    chrome.test.assertEq("0", metadata.rawTags[0].tags["minor_version"]);

    chrome.test.assertEq("h264", metadata.rawTags[1].type);
    chrome.test.assertEq("2014-02-11T00:39:25.000000Z",
                         metadata.rawTags[1].tags["creation_time"]);
    chrome.test.assertEq("VideoHandle",
                         metadata.rawTags[1].tags["handler_name"]);
    chrome.test.assertEq("eng", metadata.rawTags[1].tags["language"]);
    chrome.test.assertEq("90", metadata.rawTags[1].tags["rotate"]);

    chrome.test.assertEq("aac", metadata.rawTags[2].type);
    chrome.test.assertEq("2014-02-11T00:39:25.000000Z",
                         metadata.rawTags[2].tags["creation_time"]);
    chrome.test.assertEq("SoundHandle",
                         metadata.rawTags[2].tags["handler_name"]);
    chrome.test.assertEq("eng", metadata.rawTags[2].tags["language"]);

    chrome.test.assertEq(0, metadata.attachedImages.length);

    chrome.test.succeed();
  }

  return RunMetadataTest("90rotation.mp4", {}, verifyMetadata);
}

chrome.test.getConfig(function(config) {
  var customArg = JSON.parse(config.customArg);
  var useProprietaryCodecs = customArg[0];

  // Should still be able to sniff MP3 MIME type without proprietary codecs.
  var testsToRun = [
    ImageMIMETypeOnlyTest,
    InvalidMultimediaFileTest
  ];

  if (useProprietaryCodecs) {
    testsToRun = testsToRun.concat([
      MP3MIMETypeOnlyTest,
      MP3TagsTest,
      MP3AttachedImageTest,
      RotatedVideoTest
    ]);
  }

  chrome.test.runTests(testsToRun);
});
