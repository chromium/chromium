/**
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** @private */
var kDatabaseName = 'WebRTC-Database';

/**
 * The one and only |IDBDatabase| in this page.
 * @private
 */
var gDatabase = null;

/**
 * Set by |generateAndCloneCertificate|.
 */
var gCertificate = null;
var gCertificateClone = null;

// Public interface to tests. These are expected to be called with
// ExecuteJavascript invocations from the browser tests and will return answers
// through the DOM automation controller.

function openDatabase() {
  if (gDatabase !== null)
    throw failTest('The database is already open.');
  var reqOpen = indexedDB.open(kDatabaseName);
  reqOpen.onupgradeneeded = function() {
    // This happens before |onsuccess| if the database is new or its version is
    // updated. Create object stores.
    var db = reqOpen.result;
    var certStore = db.createObjectStore('certificates', { keyPath: 'id' });
  };
  reqOpen.onsuccess = function() {
    if (gDatabase !== null)
      failTest('The database is already open.');
    gDatabase = reqOpen.result;
    returnToTest('ok-database-opened');
  }
  reqOpen.onerror = function() {
    failTest('The database could not be opened. Error: ' + reqOpen.error);
  }
}

function closeDatabase() {
  if (gDatabase === null)
    throw failTest('The database is already closed.');
  gDatabase.close();
  gDatabase = null;
  returnToTest('ok-database-closed');
}

function deleteDatabase() {
  if (gDatabase !== null)
    throw failTest('The database should be closed before deleting.');
  var reqDelete = indexedDB.deleteDatabase(kDatabaseName);
  reqDelete.onsuccess = function () {
    returnToTest('ok-database-deleted');
  };
  reqDelete.onerror = function () {
    failTest('The database could not be deleted. Error: ' + reqDelete.error);
  };
}

/**
 * Generates a certificate and clones it by saving and loading it to the
 * database (requires database to be open, see |openDatabase|). After returning
 * successfully to the test, the global variables |gCertificate| and
 * |gCertificateClone| have been set.
 * @param {!Object} keygenAlgorithm An |AlgorithmIdentifier| to be used as
 * parameter to |RTCPeerConnection.generateCertificate|. The resulting
 * certificate will be used by the peer connection.
 */
function generateAndCloneCertificate(keygenAlgorithm) {
  RTCPeerConnection.generateCertificate(keygenAlgorithm).then(
      function(certificate) {
        gCertificate = certificate;
        if (gCertificate.getFingerprints().length == 0)
          throw failTest('getFingerprints() is empty.');
        for (let i = 0; i < gCertificate.getFingerprints().length; ++i) {
          if (gCertificate.getFingerprints()[i].algorithm != 'sha-256')
            throw failTest('Unexpected fingerprint algorithm.');
          if (gCertificate.getFingerprints()[i].value.length == 0)
            throw failTest('Unexpected fingerprint value.');
        }
        cloneCertificate_(gCertificate).then(
            function(clone) {
              let cloneIsEqual = (clone.getFingerprints().length ==
                                  gCertificate.getFingerprints().length);
              if (cloneIsEqual) {
                for (let i = 0; i < clone.getFingerprints().length; ++i) {
                  if (clone.getFingerprints()[i].algorithm !=
                      gCertificate.getFingerprints()[i].algorithm ||
                      clone.getFingerprints()[i].value !=
                      gCertificate.getFingerprints()[i].value) {
                    cloneIsEqual = false;
                    break;
                  }
                }
              }
              if (!cloneIsEqual) {
                throw failTest('The cloned certificate\'s fingerprints does ' +
                               'not match the original certificate.');
              }

              gCertificateClone = clone;
              returnToTest('ok-generated-and-cloned');
            },
            function() {
              failTest('Error cloning certificate.');
            });
      },
      function() {
        failTest('Certificate generation failed. keygenAlgorithm: ' +
            JSON.stringify(keygenAlgorithm));
      });
}

// Internals.

/** @private */
function saveCertificate_(certificate) {
  return new Promise(function(resolve, reject) {
    if (gDatabase === null)
      throw failTest('The database is not open.');

    var certTrans = gDatabase.transaction('certificates', 'readwrite');
    var certStore = certTrans.objectStore('certificates');
    var certPut = certStore.put({
      id:0,
      cert:certificate
    });

    certPut.onsuccess = function() {
      resolve();
    };
    certPut.onerror = function() {
      reject(certPut.error);
    };
  });
}

/** @private */
function loadCertificate_() {
  return new Promise(function(resolve, reject) {
    if (gDatabase === null)
      throw failTest('The database is not open.');

    var certTrans = gDatabase.transaction('certificates', 'readonly');
    var certStore = certTrans.objectStore('certificates');

    var reqGet = certStore.get(0);
    reqGet.onsuccess = function() {
      var match = reqGet.result;
      if (match !== undefined) {
        resolve(match.cert);
      } else {
        resolve(null);
      }
    };
    reqGet.onerror = function() {
      reject(reqGet.error);
    };
  });
}

/** @private */
function cloneCertificate_(certificate) {
  return saveCertificate_(certificate)
    .then(loadCertificate_)
    .then(function(clone) {
      // Save + load successful.
      if (clone === null)
        failTest('loadCertificate returned a null certificate.');
      return clone;
    });
}
