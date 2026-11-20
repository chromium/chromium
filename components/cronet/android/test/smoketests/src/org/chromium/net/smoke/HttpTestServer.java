// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import org.chromium.base.Log;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

/** A simple HTTP/1.1 server for testing. It answers every request with "Hello!". */
public class HttpTestServer implements TestSupport.TestServer {
    private static final String TAG = HttpTestServer.class.getSimpleName();
    // Connections are served one at a time, so a client that connects and never sends a request
    // (a preconnect, for example) is dropped after this long instead of blocking the next one.
    private static final int READ_TIMEOUT_MS = 5000;
    private static final byte[] RESPONSE =
            ("HTTP/1.1 200 OK\r\n"
                            + "Content-Length: 6\r\n"
                            + "Connection: close\r\n"
                            + "\r\n"
                            + "Hello!")
                    .getBytes(StandardCharsets.US_ASCII);

    private ServerSocket mServerSocket;
    private volatile Socket mClientSocket;
    private Thread mServerThread;

    @Override
    public boolean start() {
        ServerSocket serverSocket;
        try {
            serverSocket = new ServerSocket(0);
        } catch (IOException e) {
            Log.e(TAG, "Unable to start HttpTestServer", e);
            return false;
        }
        mServerSocket = serverSocket;
        mServerThread = new Thread(() -> serve(serverSocket));
        mServerThread.start();
        return true;
    }

    @Override
    public void shutdown() {
        if (mServerSocket == null) return;
        try {
            mServerSocket.close();
            Socket clientSocket = mClientSocket;
            if (clientSocket != null) clientSocket.close();
            mServerThread.join();
        } catch (IOException | InterruptedException e) {
            Log.e(TAG, "Unable to shutdown the server", e);
        }
        mServerSocket = null;
        mServerThread = null;
    }

    @Override
    public String getSuccessURL() {
        return "http://127.0.0.1:" + mServerSocket.getLocalPort() + "/success";
    }

    private void serve(ServerSocket serverSocket) {
        while (true) {
            try (Socket socket = serverSocket.accept()) {
                mClientSocket = socket;
                socket.setSoTimeout(READ_TIMEOUT_MS);
                respond(socket);
            } catch (IOException e) {
                if (serverSocket.isClosed()) return;
                Log.e(TAG, "Unable to answer a request", e);
            } finally {
                mClientSocket = null;
            }
        }
    }

    private static void respond(Socket socket) throws IOException {
        BufferedReader reader =
                new BufferedReader(
                        new InputStreamReader(socket.getInputStream(), StandardCharsets.US_ASCII));
        // The smoke tests only send GET requests, so the request ends with the empty line after
        // the headers.
        String line = reader.readLine();
        while (line != null && !line.isEmpty()) {
            line = reader.readLine();
        }
        OutputStream output = socket.getOutputStream();
        output.write(RESPONSE);
        output.flush();
    }
}
