// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var TestConstants = {
  isPowerwashed: 0,
  wallpaperUrl: 'https://test.com/test.jpg',
  highResolutionSuffix: 'suffix',
  // A dummy string which is used to mock an image.
  IMAGE: '*#*@#&',
  // A dummy array which is used to mock the file content.
  FILESTRING: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
};

// mock FileReader object in HTML5 File System
function FileReader() {
  this.result = '';
  this.onloadend = function() {
  };
  this.readAsArrayBuffer = function(mockFile) {
    this.result = mockFile;
    this.onloadend();
  }
}

// Mock localFS handler
var mockLocalFS = {
  root: {
    dirList: [],
    rootFileList: [],
    getDirectory: function(dir, isCreate, success, failure) {
      for(var i = 0; i < this.dirList.length; i++) {
        if (this.dirList[i].name == dir) {
          success(this.dirList[i]);
          return;
        }
      }
      if (!isCreate.create) {
        if (failure)
          failure('DIR_NOT_FOUND');
      } else {
        this.dirList.push(new DirEntry(dir));
        success(this.dirList[this.dirList.length - 1]);
      }
    },
    getFile: function(fileName, isCreate, success, failure) {
      if (fileName[0] == '/')
        fileName = fileName.substr(1);
      if (fileName.split('/').length == 1) {
        for(var i = 0; i < this.rootFileList.length; i++) {
          if (fileName == this.rootFileList[i].name) {
            success(this.rootFileList[i]);
            return;
          }
        }
        if (!isCreate.create) {
          if (failure)
            failure('FILE_NOT_FOUND');
        } else {
          this.rootFileList.push(new FileEntry(fileName));
          success(this.rootFileList[this.rootFileList.length - 1]);
        }
      } else if (fileName.split('/').length == 2) {
        var realDirName = fileName.split('/')[0];
        var realFileName = fileName.split('/')[1];
        var getDirSuccess = function(dirEntry) {
          dirEntry.getFile(realFileName, isCreate, success, failure);
        };
        this.getDirectory(realDirName, {create: false},
                                 getDirSuccess, failure);
      } else {
        console.error('Only support one level deep subdirectory')
      }
    }
  },
  /**
   * Create a new file in mockLocalFS.
   * @param {string} fileName File name that to be created.
   * @return {FileEntry} Handle of the new file
   */
  mockTestFile: function(fileName) {
    var mockFile;
    if (fileName[0] == '/')
      fileName = fileName.substr(1);
    if (fileName.split('/').length == 1) {
      mockFile = new FileEntry(fileName);
      this.root.rootFileList.push(mockFile);
    } else if (fileName.split('/').length == 2) {
      var realDirName = fileName.split('/')[0];
      var realFileName = fileName.split('/')[1];
      var getDirSuccess = function(dirEntry) {
        dirEntry.getFile(realFileName, {create: true},
                         function(fe) {mockFile = fe;} );
      };
      this.root.getDirectory(realDirName, {create: true}, getDirSuccess);
    } else {
      console.error('Only support one-level fileSystem mock')
    }
    return mockFile;
  },
  /**
   * Delete all files and directories in mockLocalFS.
   */
  reset: function() {
    this.root.dirList = [];
    this.root.rootFileList = [];
  }
};

function DirEntry(dirname) {
  this.name = dirname;
  this.fileList = [];
  this.getFile = function(fileName, isCreate, success, failure) {
    for(var i = 0; i < this.fileList.length; i++) {
      if (fileName == this.fileList[i].name) {
        success(this.fileList[i]);
        return;
      }
    }
    if (!isCreate.create) {
      if (failure)
        failure('FILE_NOT_FOUND');
    } else {
      this.fileList.push( new FileEntry(fileName) );
      success(this.fileList[this.fileList.length - 1]);
    }
  }
}

window.webkitRequestFileSystem = function(type, size, callback) {
  callback(mockLocalFS);
}

function Blob(arg) {
  var data = arg[0];
  this.content = '';
  if (typeof data == 'string')
    this.content = data;
  else
    this.content = Array.prototype.join.call(data);
}

var mockWriter = {
  write: function(blobData) {
  }
};

function FileEntry(filename) {
  this.name = filename;
  this.file = function(callback) {
    callback(TestConstants.FILESTRING);
  };
  this.createWriter = function(callback) {
    callback(mockWriter);
  };
  this.remove = function(success, failure) {
  };
}

// Mock chrome syncFS handler
var mockSyncFS = {
  root: {
    fileList: [],
    getFile: function(fileName, isCreate, success, failure) {
      for(var i = 0; i < this.fileList.length; i++) {
        if (fileName == this.fileList[i].name) {
          success(this.fileList[i]);
          return;
        }
      }
      if (!isCreate.create) {
        if (failure)
          failure('FILE_NOT_FOUND');
      } else {
        this.fileList.push(new FileEntry(fileName));
        success(this.fileList[this.fileList.length - 1]);
      }
    },
  },
  /**
   * Create a new file in mockSyncFS.
   * @param {string} fileName File name that to be created.
   * @return {FileEntry} Handle of the new file
   */
  mockTestFile: function(fileName) {
    var mockFile = new FileEntry(fileName);
    this.root.fileList.push(mockFile);
    return mockFile;
  },
  /**
   * Delete all files in mockSyncFS.
   */
  reset: function() {
    this.root.fileList = [];
  }
};

// Mock a few chrome apis.
var chrome = {
  storage: {
    local: {
      get: function(key, callback) {
        var items = {};
        switch (key) {
          case Constants.AccessLocalSurpriseMeEnabledKey:
            items[Constants.AccessLocalSurpriseMeEnabledKey] = true;
            break;
          case Constants.AccessLocalWallpaperInfoKey:
            if (TestConstants.isPowerwashed) {
              items[Constants.AccessLocalWallpaperInfoKey] = null;
            } else {
              items[Constants.AccessLocalWallpaperInfoKey] = {
                'url': TestConstants.wallpaperUrl,
                'layout': 'dummy',
                'source': Constants.WallpaperSourceEnum.Custom
              };
            }
            break;
          case Constants.AccessLocalManifestKey:
            items[Constants.AccessLocalManifestKey] = {
              'wallpaper_list': [{
                'available_for_surprise_me': true,
                'base_url': TestConstants.wallpaperUrl,
                'default_layout': 'dummy'
              }]
            };
            break;
        }
        callback(items);
      },
      set: function(items, callback) {}
    },
    sync: {
      get: function(key, callback) {
        var items = {};
        switch (key) {
          case Constants.AccessSyncSurpriseMeEnabledKey:
            items[Constants.AccessSyncSurpriseMeEnabledKey] = true;
            break;
          case Constants.AccessLastSurpriseWallpaperChangedDate:
            items[Constants.AccessLastSurpriseWallpaperChangedDate] =
                new Date().toDateString();
            break;
        }
        callback(items);
      },
      set: function(items, callback) {}
    },
    onChanged: {
      addListener: function(listener) {
        this.dispatch = listener;
      }
    }
  },
  syncFileSystem: {
    requestFileSystem: function(callback) {
      callback(mockSyncFS);
    },
    onFileStatusChanged: {
      addListener: function(listener) {
        this.dispatch = listener;
      }
    }
  },
  app: {runtime: {onLaunched: {addListener: function(listener) {}}}},
  alarms: {onAlarm: {addListener: function(listener) {}}},
  wallpaperPrivate: {
    getStrings: function(callback) {
      callback({
        highResolutionSuffix: TestConstants.highResolutionSuffix
      });
    },
    setCustomWallpaper: function(
        data, layout, generateThumbnail, fileName, previewMode, callback) {},
    getSyncSetting: function(callback) {
      var setting = {};
      setting.syncThemes = true;
      callback(setting);
    },
    onWallpaperChangedBy3rdParty: {addListener: function(listener) {}},
    getCollectionsInfo: function(callback) {
      callback([{collectionId: 'dummyId'}]);
    },
    getImagesInfo: function(collectionId, callback) {
      callback([{imageUrl: TestConstants.wallpaperUrl}]);
    },
    getSurpriseMeImage: function(collectionId, resumeToken, callback) {
      callback(
          {imageUrl: TestConstants.wallpaperUrl}, null /*nextResumeToken=*/);
    }
  },
  runtime: {lastError: null}
};

(function (exports) {
  var originalXMLHttpRequest = window.XMLHttpRequest;

  // Mock XMLHttpRequest. It dispatches a 'load' event immediately with status
  // equals to 200 in function |send|.
  function MockXMLHttpRequest() {
  }

  MockXMLHttpRequest.prototype = {
    responseType: null,
    url: null,

    send: function(data) {
      this.status = 200;
      this.dispatchEvent('load');
    },

    addEventListener: function(type, listener) {
      this.eventListeners = this.eventListeners || {};
      this.eventListeners[type] = this.eventListeners[type] || [];
      this.eventListeners[type].push(listener);
    },

    removeEventListener: function (type, listener) {
      var listeners = this.eventListeners && this.eventListeners[type] || [];

      for (var i = 0; i < listeners.length; ++i) {
        if (listeners[i] == listener)
          return listeners.splice(i, 1);
      }
    },

    dispatchEvent: function(type) {
      var listeners = this.eventListeners && this.eventListeners[type] || [];

      if (/test.jpg$/g.test(this.url))
        this.response = TestConstants.IMAGE;
      else
        this.response = '';

      for (var i = 0; i < listeners.length; ++i)
        listeners[i].call(this, new Event(type));
    },

    open: function(method, url, async) {
      this.url = url;
    }
  };

  function installMockXMLHttpRequest() {
    window['XMLHttpRequest'] = MockXMLHttpRequest;
  };

  function uninstallMockXMLHttpRequest() {
    window['XMLHttpRequest'] = originalXMLHttpRequest;
  };

  exports.installMockXMLHttpRequest = installMockXMLHttpRequest;
  exports.uninstallMockXMLHttpRequest = uninstallMockXMLHttpRequest;

})(window);
