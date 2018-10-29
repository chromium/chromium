package org.chromium;

import org.eclipse.jetty.websocket.WebSocket;
import org.eclipse.jetty.websocket.WebSocketServlet;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArraySet;

import javax.net.SocketFactory;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

public class IRCProxyWebSocket extends WebSocketServlet {

  private static final long serialVersionUID = 1L;

  private final Set<ChatWebSocket> members_ =
      new CopyOnWriteArraySet<ChatWebSocket>();

  protected void doGet(HttpServletRequest request, HttpServletResponse response) 
      throws ServletException ,IOException {
    getServletContext().getNamedDispatcher("default").forward(request,response);
  }
  
  protected WebSocket doWebSocketConnect(HttpServletRequest request,
                                         String protocol) {
    return new ChatWebSocket(); 
  }
  
  class ChatWebSocket implements WebSocket, Runnable {
    Outbound outbound_;
    Socket socket_ = null;
    OutputStreamWriter out_;
    BufferedReader in_;
    Thread thread_;
    byte frame_ = 0;

    public void onConnect(Outbound outbound) {
      outbound_= outbound;
    }
      
    public void onMessage(byte frame, byte[] data,int offset, int length) {}

    public void onMessage(byte frame, String data) {
      try {
        if (socket_ == null) {
          try {
            // We assume the client is going to connect and initiate a
            // connection with the message "server:port".
            String tokens[] = data.split(":");
            socket_ = SocketFactory.getDefault().createSocket(tokens[0],
                Integer.parseInt(tokens[1]));
            out_ = new OutputStreamWriter(socket_.getOutputStream());
            InputStreamReader in = new InputStreamReader(
                socket_.getInputStream());
            in_ = new BufferedReader(in);

            members_.add(this);
            thread_ = new Thread(this);
            thread_.start();
            
          } catch (UnknownHostException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
          } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
          } 
        } else {
          System.out.print(">> " + data);
          out_.write(data);
          out_.flush();
        }        
      } catch (IOException e) {
        // TODO Auto-generated catch block
        e.printStackTrace();
      }
    }

    public void onDisconnect() {
      try {
        socket_.close();
        thread_.stop();
      } catch (IOException e) {
        // TODO Auto-generated catch block
        e.printStackTrace();
      }
      members_.remove(this);
    }

    @Override
    public void run() {
      while(true) {
        try {
           if (in_.ready()) {
            String line = in_.readLine();
            System.out.println("<< " + line);
            outbound_.sendMessage(frame_, line + "\r\n");
           
          } else {
            Thread.sleep(100);
          }
        } catch (IOException e) {
        } catch (InterruptedException e) {
        }
      }
    }
  }
}
