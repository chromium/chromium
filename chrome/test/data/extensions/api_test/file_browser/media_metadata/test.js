// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test files should be created before running the tests.
 */
let audioEntry = null;
let brokeEntry = null;
let emptyEntry = null;
let imageEntry = null;
let videoEntry = null;

/*
 * getContentMineType of an empty entry is undefined.
 */
function testGetContentMimeTypeEmpty() {
  const entry = emptyEntry;

  chrome.fileManagerPrivate.getContentMimeType(entry, (mimeType) => {
    chrome.test.assertEq(undefined, mimeType);
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  });
}

/*
 * getContentMineType detects content mime types: image.
 */
function testGetContentMimeTypeImage() {
  const entry = imageEntry;

  chrome.fileManagerPrivate.getContentMimeType(entry, (mimeType) => {
    chrome.test.assertEq('image/jpeg', mimeType);
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  });
}

/*
 * getContentMineType detects content mime types: audio.
 */
function testGetContentMimeTypeAudio() {
  const entry = audioEntry;

  chrome.fileManagerPrivate.getContentMimeType(entry, (mimeType) => {
    chrome.test.assertEq('audio/mpeg', mimeType);
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  });
}

/*
 * getContentMineType detects content mime types: video.
 */
function testGetContentMimeTypeVideo() {
  const entry = videoEntry;

  chrome.fileManagerPrivate.getContentMimeType(entry, (mimeType) => {
    chrome.test.assertEq('video/mp4', mimeType);
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  });
}

/*
 * getContentMineType can fail to detect the blob content mime type and sets
 * chrome.runtime.lastError in that case.
 */
function testGetContentMimeTypeUnknownMimeTypeError() {
  const entry = brokeEntry;

  chrome.fileManagerPrivate.getContentMimeType(entry, (mimeType) => {
    chrome.test.assertEq(undefined, mimeType);

    if (!chrome.runtime.lastError) {
      chrome.test.fail('chrome.runtime.lastError expected.');
    } else {
      chrome.test.succeed();
    }
  });
}

/*
 * Helper to verify the audio test file metadata.
 */
function verifyExpectedAudioMetadata(metadata) {
  chrome.test.assertEq('Airbag', metadata.title);
  chrome.test.assertEq('Radiohead', metadata.artist);
  chrome.test.assertEq('OK Computer', metadata.album);
  chrome.test.assertEq('Alternative', metadata.genre);
  chrome.test.assertEq('Other', metadata.comment);
  chrome.test.assertEq(1.018776, metadata.duration);
  chrome.test.assertEq(1, metadata.track);

  // The file has 3 container streams: mp3 meta, mp3 audio, png image.
  chrome.test.assertEq(3, metadata.rawTags.length);

  chrome.test.assertEq('mp3', metadata.rawTags[0].type);
  chrome.test.assertEq('OK Computer', metadata.rawTags[0].tags['album']);
  chrome.test.assertEq('Radiohead', metadata.rawTags[0].tags['artist']);
  chrome.test.assertEq('1997', metadata.rawTags[0].tags['date']);
  chrome.test.assertEq('Lavf54.4.100', metadata.rawTags[0].tags['encoder']);
  chrome.test.assertEq('Alternative', metadata.rawTags[0].tags['genre']);
  chrome.test.assertEq('Airbag', metadata.rawTags[0].tags['title']);
  chrome.test.assertEq('1', metadata.rawTags[0].tags['track']);
  chrome.test.assertEq('mp3', metadata.rawTags[1].type);

  // File contains an embedded album artwork thumbnail image.
  chrome.test.assertEq('png', metadata.rawTags[2].type);

  // Embedded images do not set the video-only width and height fields.
  chrome.test.assertEq(undefined, metadata.width);
  chrome.test.assertEq(undefined, metadata.height);
}

/*
 * getContentMetadata of an empty entry is undefined.
 */
function testGetContentMetadataEmpty() {
  const entry = emptyEntry;

  chrome.fileManagerPrivate.getContentMetadata(
      entry, 'audio/mpeg', false, (metadata) => {
    chrome.test.assertEq(undefined, metadata);
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  });
}

/*
 * getContentMetadata can return metadata tags only.
 */
function testGetContentMetadataAudioTags() {
  const entry = audioEntry;

  chrome.fileManagerPrivate.getContentMetadata(
      entry, 'audio/mpeg', false, (metadata) => {
    chrome.test.assertEq('audio/mpeg', metadata.mimeType);
    chrome.test.assertNoLastError();

    verifyExpectedAudioMetadata(metadata);
    chrome.test.assertEq(0, metadata.attachedImages.length);

    chrome.test.succeed();
  });
}

/*
 * getContentMetadata can return metadata tags and metadata images.
 */
function testGetContentMetadataAudioTagsImages() {
  const entry = audioEntry;

  chrome.fileManagerPrivate.getContentMetadata(
      entry, 'audio/mpeg', true, (metadata) => {
    chrome.test.assertEq('audio/mpeg', metadata.mimeType);
    chrome.test.assertNoLastError();

    verifyExpectedAudioMetadata(metadata);
    chrome.test.assertEq(1, metadata.attachedImages.length);

    const data = metadata.attachedImages[0].data;
    if (!data || !data.startsWith('data:image/png;base64,')) {
      chrome.test.fail('Attached image expected.');
    } else {
      const type = metadata.attachedImages[0].type;
      chrome.test.assertEq('image/png', type);
      chrome.test.succeed();
    }
  });
}

/*
 * Helper to verify the video test file metadata.
 */
function verifyExpectedVideoMetadata(metadata) {
  chrome.test.assertEq(1920, metadata.width);
  chrome.test.assertEq(1080, metadata.height);

  chrome.test.assertEq(0.196056, metadata.duration);
  chrome.test.assertEq(90, metadata.rotation);
  chrome.test.assertEq('eng', metadata.language);

  // The file has 3 container streams: video meta, h264 video, aac audio.
  chrome.test.assertEq(3, metadata.rawTags.length);

  chrome.test.assertEq('mov,mp4,m4a,3gp,3g2,mj2', metadata.rawTags[0].type);
  chrome.test.assertEq('isom3gp4',
                       metadata.rawTags[0].tags['compatible_brands']);
  chrome.test.assertEq('2014-02-11T00:39:25.000000Z',
                       metadata.rawTags[0].tags['creation_time']);
  chrome.test.assertEq('isom', metadata.rawTags[0].tags['major_brand']);
  chrome.test.assertEq('0', metadata.rawTags[0].tags['minor_version']);

  chrome.test.assertEq('h264', metadata.rawTags[1].type);
  chrome.test.assertEq('2014-02-11T00:39:25.000000Z',
                       metadata.rawTags[1].tags['creation_time']);
  chrome.test.assertEq('VideoHandle',
                       metadata.rawTags[1].tags['handler_name']);
  chrome.test.assertEq('eng', metadata.rawTags[1].tags['language']);
  chrome.test.assertEq('90', metadata.rawTags[1].tags['rotate']);

  chrome.test.assertEq('aac', metadata.rawTags[2].type);
  chrome.test.assertEq('2014-02-11T00:39:25.000000Z',
                       metadata.rawTags[2].tags['creation_time']);
  chrome.test.assertEq('SoundHandle',
                       metadata.rawTags[2].tags['handler_name']);
  chrome.test.assertEq('eng', metadata.rawTags[2].tags['language']);
}

/*
 * getContentMetadata returns tags and images of a video file. Note: the test
 * video file has no attached images.
 */
function testGetContentMetadataVideoTagsImages() {
  const entry = videoEntry;

  chrome.fileManagerPrivate.getContentMetadata(
      entry, 'video/mp4', true, (metadata) => {
    chrome.test.assertEq('video/mp4', metadata.mimeType);
    chrome.test.assertNoLastError();

    verifyExpectedVideoMetadata(metadata);
    chrome.test.assertEq(0, metadata.attachedImages.length);

    chrome.test.succeed();
  });
}

/*
 * getContentMetadata returns the input mime type in the metadata mime type.
 */
function testGetContentMetadataRetainsInputMimeType() {
  const entry = audioEntry;

  chrome.fileManagerPrivate.getContentMetadata(
      entry, 'audio/input-type', false, (metadata) => {
    chrome.test.assertEq('audio/input-type', metadata.mimeType);
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  });
}

/*
 * getContentMetadata of a video file resets audio/type mime to video/type if
 * the video has width and height.
 */
function testGetContentMetadataVideoResetsAudioMime() {
  const entry = videoEntry;

  chrome.fileManagerPrivate.getContentMetadata(
      entry, 'audio/input-type', true, (metadata) => {
    chrome.test.assertEq('video/input-type', metadata.mimeType);
    chrome.test.assertNoLastError();

    chrome.test.assertEq(1920, metadata.width);
    chrome.test.assertEq(1080, metadata.height);

    chrome.test.succeed();
  });
}

/*
 * getContentMetadata supports audio and video mime types only, and will set
 * chrome.runtime.lastError given other mime types.
 */
function testGetContentMetadataUnsupportedMimetypeError() {
  const entry = imageEntry;

  chrome.fileManagerPrivate.getContentMetadata(
      entry, 'image/jpeg', false, (metadata) => {
    chrome.test.assertEq(undefined, metadata);

    if (!chrome.runtime.lastError) {
      chrome.test.fail('chrome.runtime.lastError expected.');
    } else {
      chrome.test.succeed();
    }
  });
}

/*
 * Resolves the test fileSystem.
 */
function resolveTestFileSystem() {
  return new Promise((resolve) => {
    const testVolumeType = 'testing';

    chrome.fileManagerPrivate.getVolumeMetadataList(list => {
      const volume = list.find(v => v.volumeType === testVolumeType);
      if (!volume) {
        chrome.test.fail('Failed to find testing volume.');
      }

      const volumeId = {volumeId: volume.volumeId};
      chrome.fileSystem.requestFileSystem(volumeId, (fileSystem) => {
        if (!fileSystem) {
          chrome.test.fail('Failed to acquire fileSystem.');
        }
        resolve(fileSystem);
      });
    });
  });
}

/*
 * Resolves the fileEntry for |fileName|.
 */
function resolveFileEntry(fileSystem, fileName) {
  return new Promise((resolve) => {
    const failure = (error) => {
      chrome.test.fail('While resolving ' + fileName + ': ' + error);
    };

    fileSystem.root.getFile(fileName, {}, resolve, failure);
  });
}

resolveTestFileSystem().then(async (fileSystem) => {
  audioEntry = await resolveFileEntry(fileSystem, 'id3_png_test.mp3');
  brokeEntry = await resolveFileEntry(fileSystem, 'broken.jpg');
  emptyEntry = await resolveFileEntry(fileSystem, 'empty.txt');
  imageEntry = await resolveFileEntry(fileSystem, 'image3.jpg');
  videoEntry = await resolveFileEntry(fileSystem, '90rotation.mp4');

  chrome.test.runTests([
    // fileManagerPrivate.getContentMimeType tests.
    testGetContentMimeTypeEmpty,
    testGetContentMimeTypeImage,
    testGetContentMimeTypeAudio,
    testGetContentMimeTypeVideo,
    testGetContentMimeTypeUnknownMimeTypeError,

    // fileManagerPrivate.getContentMetadata tests.
    testGetContentMetadataEmpty,
    testGetContentMetadataAudioTags,
    testGetContentMetadataAudioTagsImages,
    testGetContentMetadataVideoTagsImages,
    testGetContentMetadataRetainsInputMimeType,
    testGetContentMetadataVideoResetsAudioMime,
    testGetContentMetadataUnsupportedMimetypeError,
  ]);
});
