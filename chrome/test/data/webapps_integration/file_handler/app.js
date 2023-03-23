// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


const makeFileViewer = async (launchFile) => {
  const readHandle = await launchFile.getFile();

  const template = document.getElementById("file-viewer-template");
  const fullName = readHandle.name;
  const element = template.content.cloneNode(true);
  const nameContainer = element.querySelector("[name='file-name']");
  nameContainer.innerText = fullName;

  const contentContainer = element.querySelector("[name='file-contents']");
  contentContainer.innerText = "Loading...";
  // Asynchronously load in the file contents.
  try {
    contentContainer.value = await readHandle.text();
  } catch (err) {
    console.log(`Failed to load contents for file: ${readHandle.name}`, err);
  }

  return element;
};

var launchFinishedPromise = new Promise(resolve => {
  window.addEventListener("load", () => {
    window.launchQueue.setConsumer(async (launchParams) => {
      console.log("Launched with: ", launchParams);
      const viewersContainer = document.getElementById("viewers-container");
      if (!launchParams.files.length) {
        viewersContainer.innerText =
          "Oh poo, no files. Consider granting the permission next time!";
        return;
      }

      const openedFilesContainer = document.getElementById(
        "opened-files-container"
      );
      for (const launchFile of launchParams.files) {
        const editor = await makeFileViewer(launchFile);
        openedFilesContainer.appendChild(editor);
      }
      let results = [];
      const fileContents = openedFilesContainer.querySelectorAll(
        "[name='file-contents']");
      for (const fileContent of fileContents) {
        results.push(fileContent.value);
      }
      resolve(results);
    });
  });
});
